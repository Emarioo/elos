
#include "elos/network.h"

#include "elos/kernel/net/i8254x.h"

#include "elos/kernel/net/protocol.h"

#include "elos/kernel_console.h"

#include "elos/common/string.h"



#define printf(...) KCON_printf(__VA_ARGS__)


u32 current_ip;
u8 current_mac[6]; // set by card_init (at the moment)

void NET_init() {
    char buffer0[24];

    current_ip = ipv4_from_str("192.168.100.54");
    printf("Hardcoded IP: %s\n", ipv4_int_str(current_ip, buffer0));

    card_init();

}


void NET_poll() {
    receive_packets();
}

void handle_packet(void* buffer, int length) {
    
    // @NOCHECKIN We need to check that lengths specified in packet doesn't extend the length of the whole packet. Malicious or corrupt packet should be dropped if so.

    EtherFrame* frame = buffer;

    frame->etherType = bswap16(frame->etherType);

    switch (frame->etherType) {
        case ETHER_ARP: {
            ARP_Header* arp = (ARP_Header*)((char*)buffer + sizeof(EtherFrame));

            arp->hardware_type = bswap16(arp->hardware_type);
            arp->protocol_type = bswap16(arp->protocol_type);
            arp->operation     = bswap16(arp->operation);
            
            if (arp->hardware_type == ARP_ETHERNET && arp->protocol_type == ETHER_IPV4) {
                char buffer0[20];
                char buffer1[20];
                char buffer2[20];
                char buffer3[20];
                ARP_Header_EthernetIPV4* arp_ipv4 = (ARP_Header_EthernetIPV4*)arp;
                printf("ARP hw=%s proto=%s oper=%s senderMAC=%s senderIP=%s targetMAC=%s targetIP=%s\n",
                    htype_str(arp_ipv4->hardware_type),
                    ether_str(arp_ipv4->protocol_type),
                    oper_str(arp_ipv4->operation),
                    mac_str(arp_ipv4->sender_hw_address, buffer0),
                    ipv4_str(arp_ipv4->sender_proto_address, buffer1),
                    mac_str(arp_ipv4->target_hw_address, buffer2),
                    ipv4_str(arp_ipv4->target_proto_address, buffer3)
                    );


                if (arp_ipv4->operation == ARP_REQUEST && *(u32*)arp_ipv4->target_proto_address == current_ip) {
                    u8 message_buffer[sizeof(EtherFrame) + sizeof(ARP_Header_EthernetIPV4)] = {};
                    EtherFrame* message_frame = (EtherFrame*)message_buffer;
                    memcpy(message_frame->destination, arp_ipv4->sender_hw_address, 6);
                    memcpy(message_frame->source, current_mac, 6);
                    message_frame->etherType = ETHER_ARP;
                    ARP_Header_EthernetIPV4* message_arp = (ARP_Header_EthernetIPV4*)(message_buffer + sizeof(EtherFrame));
                    message_arp->hardware_type = ARP_ETHERNET;
                    message_arp->protocol_type = ETHER_IPV4;
                    message_arp->hardware_length = 6;
                    message_arp->protocol_length = 4;
                    message_arp->operation = ARP_REPLY;
                    memcpy(message_arp->sender_hw_address, current_mac, 6);
                    memcpy(message_arp->sender_proto_address, &current_ip, 4);
                    memcpy(message_arp->target_hw_address, arp_ipv4->sender_hw_address, 6);
                    memcpy(message_arp->target_proto_address, arp_ipv4->sender_proto_address, 4);

                    message_frame->etherType   = bswap16(message_frame->etherType);
                    message_arp->hardware_type = bswap16(message_arp->hardware_type);
                    message_arp->protocol_type = bswap16(message_arp->protocol_type);
                    message_arp->operation     = bswap16(message_arp->operation);

                    NET_send_packet(message_buffer, sizeof(message_buffer));
                }
            } else {
                printf("ARP hw=%s proto=%s oper=%s\n",
                    htype_str(arp->hardware_type),
                    ether_str(arp->protocol_type),
                    oper_str(arp->operation)
                );
            }
        } break;
        case ETHER_IPV4: {
            IPV4_Header* ip = (IPV4_Header*)((char*)buffer + sizeof(EtherFrame));

            // header length specifies number of 32-bit words (at least 5)
            u16 computed_checksum = compute_internet_checksum(ip, ip->headerLength * 4);
 
            ip->headerChecksum = bswap16(ip->headerChecksum);
            ip->totalLength = bswap16(ip->totalLength);
            ip->identification = bswap16(ip->identification);
            ip->fragmentPart = bswap16(ip->fragmentPart);

            char buffer0[20];
            char buffer1[20];
            printf("IP size=%d foffset=%d ttl=%d proto=%s chksum=%d (computed %d) src=%s dst=%s\n",
                ip->totalLength,
                ip->fragmentPart & IPV4_FRAGMENT_OFFSET_MASK,
                ip->timeToLive,
                ipproto_str(ip->protocol),
                ip->headerChecksum,
                computed_checksum,
                ipv4_int_str(ip->sourceAddress, buffer0),
                ipv4_int_str(ip->destinationAddress, buffer1)
                );

            // switch (ip->protocol) {
            //     case IP_ICMP: {
            //         ICMP_Header* icmp = (ICMP_Header*)((char*)ip + ip->headerLength);
            //         int icmp_size = ip->totalLength - ip->headerLength;
                    
            //         u16 computed_checksum = compute_internet_checksum(icmp, icmp_size);

            //         if (icmp->type == ICMP_ECHO_REQUEST) {
            //             // @TODO Construct packet
            //         } else {
            //             // ignore for now
            //         }
            //     } break;
            // }

        } break;
        default: {
            char buffer0[20];
            char buffer1[20];
            printf("PACKET etherType=%s dst=%s src=%s\n",
                ether_str(frame->etherType),
                mac_str(frame->destination, buffer0),
                mac_str(frame->source, buffer1)
            );
        } break;
    }

}


void NET_send_packet(void* buffer, int size) {
    send_packet(buffer, size);
}
