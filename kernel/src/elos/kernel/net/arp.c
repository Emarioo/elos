#include "elos/network.h"

#include "elos/kernel/net/protocol.h"

#include "elos/common/string.h"
#include "elos/common/intrinsics.h"

typedef struct {
    u32 last_access;
    u32 address;
    u8  mac[6];
} ARP_Entry;

#define ARP_TABLE_MAX 50
int arp_table_len;
u64 rdtsc_base;
ARP_Entry arp_table[ARP_TABLE_MAX];




bool NET_unpack_arp(NET_Packet* packet, u8 mac[6]) {

    EtherFrame* frame = (void*)packet->buffer;

    frame->etherType = bswap16(frame->etherType);

    // printf("Ether typ %d\n", frame->etherType);

    if (frame->etherType == ETHER_ARP) {
        ARP_Header* arp = (ARP_Header*)((char*)packet->buffer + sizeof(EtherFrame));

        arp->hardware_type = bswap16(arp->hardware_type);
        arp->protocol_type = bswap16(arp->protocol_type);
        arp->operation     = bswap16(arp->operation);

        // printf("ARP oper %d\n", arp->operation);
        
        if (arp->hardware_type == ARP_ETHERNET && arp->protocol_type == ETHER_IPV4) {
            ARP_Header_EthernetIPV4* arp_ipv4 = (ARP_Header_EthernetIPV4*)arp;
            // uint64_t now = now_us();
            memcpy(mac, arp_ipv4->sender_hw_address, 6);
            char buffer0[30];
            // printf("Got MAC from ARP, %s, %d us\n", mac_str(mac,buffer0), (now - startTime));
            // sucess
            return true;
        }
    }
    return false;
}




bool fetch_mac_from_address(NetDevice device, u32 address, u8 mac[6]) {
    ARP_Entry* entry = NULL;
    for (int i=0;i<arp_table_len;i++) {
        if (arp_table[i].address == address) {
            entry = &arp_table[i];
            break;
        }
    }

    if (entry) {
        memcpy(mac, entry->mac, sizeof(*mac));
        return true;
    }
    
    if (rdtsc_base == 0) {
        rdtsc_base = rdtsc();
    }
    u32 timestamp = (u32)((rdtsc() - rdtsc_base) / 1000000);

    
    // Send arp request.
    NET_send_arp(device, address);

    // Wait for ARP reply with a timeout
    
    // We don't poll because an interrupt will fire and handle the packet.
    // That needs to trigger/signal that we are waiting for ARP here.
    // What if multiple arp requests are sent. The interrupt handler
    // needs to know which one we're waiting for?


    while (true) {
        NET_Packet packet;
        bool found = NET_poll_packet(device, &packet);
        if (found) {
            u8 received_mac[6];
            bool unpacked = NET_unpack_arp(&packet, received_mac);
            if (!unpacked) {
                
            }
        }
        pause();
    }

    if (arp_table_len < ARP_TABLE_MAX) {
        entry = &arp_table[arp_table_len];
        arp_table_len++;
    } else {
        
    }

    entry->last_access = timestamp;
    memcpy(mac, entry->mac, 6);
    return true;
}