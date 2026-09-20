#include "elos/network.h"
#include "elos/kernel/net/net_internal.h"

#include "elos/kernel/net/protocol.h"

#include "elos/common/string.h"
#include "elos/common/intrinsics.h"
#include "elos/cpu.h"

#include "elos/kernel_console.h"


#define printf(...) KCON_printf(__VA_ARGS__)


typedef struct {
    u32         last_access;
    u32         address;
    u8          mac[6];
    NET_Device* device;
} ARP_Entry;

#define ARP_TABLE_MAX 50
int arp_table_len;
ARP_Entry arp_table[ARP_TABLE_MAX];
volatile u32 arpTableVersion;
volatile u32 arp_table_lock;




// bool NET_unpack_arp(NET_Packet* packet, u8 mac[6]) {

//     EtherFrame* frame = (void*)packet->buffer;

//     frame->etherType = bswap16(frame->etherType);

//     // printf("Ether typ %d\n", frame->etherType);

//     if (frame->etherType == ETHER_ARP) {
//         ARP_Header* arp = (ARP_Header*)((char*)packet->buffer + sizeof(EtherFrame));

//         arp->hardware_type = bswap16(arp->hardware_type);
//         arp->protocol_type = bswap16(arp->protocol_type);
//         arp->operation     = bswap16(arp->operation);

//         // printf("ARP oper %d\n", arp->operation);
        
//         if (arp->hardware_type == ARP_ETHERNET && arp->protocol_type == ETHER_IPV4) {
//             ARP_Header_EthernetIPV4* arp_ipv4 = (ARP_Header_EthernetIPV4*)arp;
//             // uint64_t now = now_us();
//             memcpy(mac, arp_ipv4->sender_hw_address, 6);
//             char buffer0[30];
//             // printf("Got MAC from ARP, %s, %d us\n", mac_str(mac,buffer0), (now - startTime));
//             // sucess
//             return true;
//         }
//     }
//     return false;
// }


bool find_mac(u32 address, NET_Device** device, u8 mac[6]) {
    bool returnValue = false;
    LOCK_INT(&arp_table_lock);

    ARP_Entry* entry = NULL;
    for (int i=0;i<arp_table_len;i++) {
        // printf("CMP %x %x\n", arp_table[i].address, address);
        if (arp_table[i].address == address) {
            entry = &arp_table[i];
            break;
        }
    }

    if (entry) {
        *device = entry->device;
        memcpy(mac, entry->mac, sizeof(*mac));
        returnValue = true;
        goto exit;
    }

exit:
    UNLOCK_INT(&arp_table_lock);
    return returnValue;
}


void arp_update_table(u32 address, NET_Device* device, u8 mac[6]) {
    // char temp1[20];
    // char temp2[30];
    // printf("ARP update %s %p %s\n", ipv4_int_str(address, temp1), device, mac_str(mac, temp2));
    
    bool returnValue = false;
    LOCK_INT(&arp_table_lock);

    ARP_Entry* entry = NULL;
    for (int i=0;i<arp_table_len;i++) {
        if (arp_table[i].address == address) {
            entry = &arp_table[i];
            break;
        }
    }

    if (!entry && arp_table_len+1 < ARP_TABLE_MAX) {
        entry = &arp_table[arp_table_len];
        arp_table_len++;
    }

    if (entry) {
        entry->address = address;
        entry->device = device;
        memcpy(entry->mac, mac, sizeof(*mac));
        arpTableVersion++;
        returnValue = true;
    }

    UNLOCK_INT(&arp_table_lock);
}


bool fetch_mac_from_address(u32 address, NET_Device** out_device, u8 out_mac[6]) {
    bool res;

    res = find_mac(address, out_device, out_mac);
    if (res) {
        return true;
    }
    
    // Send arp request to all network controllers
    // @TODO Lock device list
    for (int i=0;i<g_devices_len;i++) {
        NET_Device* device = &g_devices[i];
        if (device->present) {
            NET_send_arp(device, address);
        }
    }

    u64 tickStart = CPU_ticks();
    u64 tps = CPU_ticks_per_second();
    u64 timeout_tick = tickStart + tps / 10;

    u32 prev_version = arpTableVersion;

    // @TODO Better sleep and rescheduling?
    //    Fine to waste CPU cycles for now i guess (makes things simple).
    while (true) {

        if (arpTableVersion != prev_version) {
            printf("Table changes!\n");
            res = find_mac(address, out_device, out_mac);
            if (res) {
                return true;
            }
            arpTableVersion = prev_version;
        }

        u64 tickNow = CPU_ticks();
        if (tickNow > timeout_tick) {
            printf("arp timeout\n");
            break;
        }

        pause();
    }

    return false;
}
