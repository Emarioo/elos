
#include "elos/network.h"

#include "elos/kernel/net/i8254x.h"

#include "elos/kernel/net/protocol.h"

#include "elos/kernel_console.h"

#include "elos/common/string.h"

#include "elos/physical_memory.h"



#define printf(...) KCON_printf(__VA_ARGS__)


u32 current_ip;
u8 current_mac[6]; // set by card_init (at the moment)

int fake_device;

void NET_scan_devices(NetDevice devices[], int* count) {
    if (!count) {
        return;
    }
    if (!devices && count) {
        // @TODO Don't hardcode.
        *count = 1;
        return;
    }
    
    char buffer0[24];

    current_ip = ipv4_from_str("192.168.100.54");
    printf("Hardcoded IP: %s\n", ipv4_int_str(current_ip, buffer0));

    card_init();

    devices[0] = &fake_device;
    *count = 1;
}

void NET_cleanup() {
    // @TODO implement
}

FN_NET_recv_packet g_recv_packet_callback;
void* g_recv_packet_callback_userData;

void NET_set_receive_callback(NetDevice device, FN_NET_recv_packet callback, void* user_data) {
    // @TODO Thread safety.
    g_recv_packet_callback = callback;
    g_recv_packet_callback_userData = user_data;
}

bool NET_poll_packet(NetDevice device, NET_Packet* packet) {
    if (!packet) {
        kernel_bug();
        return false;
    }
    
    void* buffer;
    int size;
    receive_packet(&buffer, &size);

    if (!buffer || !size)
        return false;

    packet->buffer = buffer;
    packet->size = size;
    return true;
}
void NET_free_packet(NetDevice device, NET_Packet* packet) {
    PMEM_free(packet->buffer);
    packet->buffer = NULL;
    packet->size = 0;
}

void NET_send_packet(NetDevice device, void* buffer, int size) {
    if (!device) {
        kernel_bug();
        return;
    }
    send_packet(buffer, size);
}



void NET_handle_packet(NetDevice device, NET_Packet* packet) {
    
    // @NOCHECKIN We need to check that lengths specified in packet doesn't extend the length of the whole packet. Malicious or corrupt packet should be dropped if so.

    if (!packet->buffer || !packet->size)
        return;

    void* buffer = packet->buffer;
    int size = packet->size;

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

                    NET_send_packet(device, message_buffer, sizeof(message_buffer));
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
            
            int ipHeaderSize = ip->headerLength * 4;

            switch (ip->protocol) {
                case IP_ICMP: {
                    ICMP_Header* icmp = (ICMP_Header*)((char*)ip + ipHeaderSize);
                    int icmp_size = ip->totalLength - ipHeaderSize;
                    
                    u16 computed_checksum = compute_internet_checksum(icmp, icmp_size);
                    
                    icmp->checksum = bswap16(icmp->checksum);


                    if (icmp->type == ICMP_ECHO_REQUEST) {
                        ICMP_Header_Echo* echo = (ICMP_Header_Echo*)icmp;

                        echo->identifier = bswap16(echo->identifier);
                        echo->sequence_number = bswap16(echo->sequence_number);

                        printf("ICMP type=%d code=%d chksum=%d (computed %d) ident=%d seq=%d\n",
                            echo->type,
                            echo->code,
                            echo->checksum,
                            computed_checksum,
                            echo->identifier,
                            echo->sequence_number
                            );
                        
                        if (computed_checksum != 0) {
                            // Don't send anything back, checksum is bad (or my implementation is bad)
                            return;
                        }

                        u8 message_buffer[sizeof(EtherFrame) + sizeof(IPV4_Header) + sizeof(ICMP_Header_Echo) + 256] = {};

                        int packet_size = sizeof(EtherFrame) + sizeof(IPV4_Header) + icmp_size;

                        if (packet_size > sizeof(message_buffer)) {
                            printf("NET: ICMP packet payload to big for static buffer, dropping\n");
                            return;
                        }

                        EtherFrame* message_frame = (EtherFrame*)message_buffer;
                        memcpy(message_frame->destination, frame->source, 6);
                        memcpy(message_frame->source, current_mac, 6);
                        message_frame->etherType = ETHER_IPV4;
                        IPV4_Header* message_ipv4 = (IPV4_Header*)(message_buffer + sizeof(EtherFrame));
                        message_ipv4->headerLength = sizeof(IPV4_Header) / 4;
                        message_ipv4->version = 4;
                        message_ipv4->totalLength = sizeof(IPV4_Header) + icmp_size;
                        message_ipv4->identification = ip->identification;
                        message_ipv4->fragmentPart = IPV4_FLAG_DONT_FRAGMENT;
                        message_ipv4->headerChecksum = 0;
                        message_ipv4->timeToLive = 64;
                        message_ipv4->protocol = IP_ICMP;
                        memcpy(&message_ipv4->sourceAddress, &current_ip, 4);
                        memcpy(&message_ipv4->destinationAddress, &ip->sourceAddress, 4);

                        message_frame->etherType = bswap16(message_frame->etherType);
                        message_ipv4->totalLength = bswap16(message_ipv4->totalLength);
                        message_ipv4->identification = bswap16(message_ipv4->identification);
                        message_ipv4->fragmentPart = bswap16(message_ipv4->fragmentPart);

                        message_ipv4->headerChecksum = bswap16(compute_internet_checksum(message_ipv4, sizeof(IPV4_Header)));

                        ICMP_Header_Echo* message_echo = (ICMP_Header_Echo*)(message_buffer + sizeof(EtherFrame) + sizeof(IPV4_Header));
                        message_echo->type = ICMP_ECHO_REPLY;
                        message_echo->code = 0;
                        message_echo->checksum = 0;
                        message_echo->identifier = echo->identifier;
                        message_echo->sequence_number = echo->sequence_number;

                        memcpy(message_echo->payload, echo->payload, icmp_size - sizeof(ICMP_Header_Echo));

                        message_echo->identifier = bswap16(message_echo->identifier);
                        message_echo->sequence_number = bswap16(message_echo->sequence_number);
                        
                        message_echo->checksum = bswap16(compute_internet_checksum(message_echo, icmp_size));

                        NET_send_packet(device, message_buffer, packet_size); 
                    } else {
                        // ignore for now
                    }
                } break;
                case IP_UDP: {
                    UDP_Header* udp = (UDP_Header*)((char*)ip + ipHeaderSize);
                    int udpSize = bswap16(udp->length);
                    // u16 computed_checksum = compute_internet_checksum(udp, udpSize);
                    // @TODO Ignoring checksum for now

                    udp->sourcePort = bswap16(udp->sourcePort);
                    udp->destinationPort = bswap16(udp->destinationPort);
                    udp->length = bswap16(udp->length);
                    
                    
                    u8 message_buffer[sizeof(EtherFrame) + sizeof(IPV4_Header) + sizeof(UDP_Header) + 256] = {};

                    int packet_size = sizeof(EtherFrame) + sizeof(IPV4_Header) + udpSize;

                    if (packet_size > sizeof(message_buffer)) {
                        printf("NET: UDP packet data to big for static buffer, dropping\n");
                        return;
                    }

                    int message_udpSize = sizeof(UDP_Header) + 4;

                    EtherFrame* message_frame = (EtherFrame*)message_buffer;
                    memcpy(message_frame->destination, frame->source, 6);
                    memcpy(message_frame->source, current_mac, 6);
                    message_frame->etherType = ETHER_IPV4;
                    IPV4_Header* message_ipv4 = (IPV4_Header*)(message_buffer + sizeof(EtherFrame));
                    message_ipv4->headerLength = sizeof(IPV4_Header) / 4;
                    message_ipv4->version = 4;
                    message_ipv4->totalLength = sizeof(IPV4_Header) + message_udpSize;
                    message_ipv4->identification = ip->identification;
                    message_ipv4->fragmentPart = IPV4_FLAG_DONT_FRAGMENT;
                    message_ipv4->headerChecksum = 0;
                    message_ipv4->timeToLive = 64;
                    message_ipv4->protocol = IP_UDP;
                    memcpy(&message_ipv4->sourceAddress, &current_ip, 4);
                    memcpy(&message_ipv4->destinationAddress, &ip->sourceAddress, 4);

                    message_frame->etherType = bswap16(message_frame->etherType);
                    message_ipv4->totalLength = bswap16(message_ipv4->totalLength);
                    message_ipv4->identification = bswap16(message_ipv4->identification);
                    message_ipv4->fragmentPart = bswap16(message_ipv4->fragmentPart);

                    message_ipv4->headerChecksum = bswap16(compute_internet_checksum(message_ipv4, sizeof(IPV4_Header)));

                    UDP_Header* message_udp = (UDP_Header*)(message_buffer + sizeof(EtherFrame) + sizeof(IPV4_Header));
                    message_udp->sourcePort = udp->destinationPort;
                    message_udp->destinationPort = udp->sourcePort;
                    message_udp->checksum = 0;
                    message_udp->length = message_udpSize;

                    message_udp->sourcePort = bswap16(message_udp->sourcePort);
                    message_udp->destinationPort = bswap16(message_udp->destinationPort);
                    message_udp->length = bswap16(message_udp->length);

                    int value = *(int*)udp->data;
                    value += 1;
                    *(int*)message_udp->data = value;

                    // @TODO Checksum. Need pseduo header for ipv4
                    UDP_Pseudo_Header pseudo = {};
                    pseudo.sourceAddress = message_ipv4->sourceAddress;
                    pseudo.destinationAddress = message_ipv4->destinationAddress;
                    pseudo.protocol = IP_UDP;
                    pseudo.udpLength = bswap16(message_udpSize);

                    message_udp->checksum = bswap16(~compute_internet_checksum(&pseudo, sizeof(UDP_Pseudo_Header)));

                    message_udp->checksum = bswap16(compute_internet_checksum(message_udp, message_udpSize));

                    NET_send_packet(device, message_buffer, sizeof(EtherFrame) + sizeof(IPV4_Header) + message_udpSize);

                } break;
                default: {

                } break;
            }

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

