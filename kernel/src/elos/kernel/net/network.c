
#include "elos/network.h"

#include "elos/kernel/net/i8254x.h"
#include "elos/kernel/net/rtl8169.h"

#include "elos/kernel/net/protocol.h"

#include "elos/kernel_console.h"

#include "elos/common/string.h"

#include "elos/physical_memory.h"

#include "elos/kernel/net/net_internal.h"



#define printf(...) KCON_printf(__VA_ARGS__)


u32 current_ip;
u8 current_mac[6]; // set by card_init (at the moment)

int fake_device;




NetworkController controller;


bool find_device(PCI_Scanner* scanner, PCI_ConfigSpace* config) {
    if (config->classCode == PCI_CLASSCODE__NETWORK_CONTROLLER) {
        if (config->vendorID == VENDOR_ID__INTEL && config->deviceID == DEVICE_ID__82540EM_A) {
            controller.found = true;
            controller.config = *config;
            return true;
        } else if (config->vendorID == VENDOR_ID__REALTEK && config->deviceID == DEVICE_ID__RTL8169) {
            controller.found = true;
            controller.config = *config;
            return true;
        } else {
            printf("[INFO] NET_init: Searching PCI for network card, found device id 0x%x (vendor 0x%x), but card is not supported.\n", config->deviceID, config->vendorID);
        }
    }

    return false;
}

void NET_scan_devices(NetDevice devices[], int* count) {
    if (!count) {
        // INVALID parameter
        return;
    }
    if (!devices && count) {
        // @TODO Don't hardcode.
        *count = 1;
        return;
    }
    
    char buffer0[24];

    current_ip = ipv4_from_str("192.168.100.54");
    // printf("Hardcoded IP: %s\n", ipv4_int_str(current_ip, buffer0));


    PCI_Scanner scanner = { .func = find_device };

    pci_scan_buses(&scanner);
    
    if (!controller.found) {
        printf("[WARNING] NET_init: Could not find a supported Network Controller on the PCI bus.\n");
        *count = 0;
        return;
    } else {
        
        // u32* bars = &controller.config.header0.bar0;

        // int head = 0;
        // while (head < 6) {
        //     u32 bar = bars[head];
        //     head++;

        //     if (bar & 0x1) {
        //         printf("[INFO] bar[%d] IO-mapped addr=%x\n", head, bar & ~0x3);
        //     } else if (((bar >> 1) & 0x6) == 0) {
        //         if (bar & 0x8) {
        //             printf("[INFO] bar[%d] 32-bit prefetchable addr=%x\n", head, bar & ~0xf);
        //         } else {
        //             printf("[INFO] bar[%d] 32-bit addr=%x\n", head, bar & ~0xf);
        //         }
        //     } else if (((bar >> 1) & 0x3) == 2) {
        //         u32 bar_ext = bars[head];
        //         head++;
        //         if (bar & 0x8) {
        //             printf("[INFO] bar[%d] 64-bit prefetchable\n", head, ((u64)bar_ext << 32) | ((u64)bar & ~0xfLLU));
        //         } else {
        //             printf("[INFO] bar[%d] 64-bit addr=%x\n", head, ((u64)bar_ext << 32) | ((u64)bar & ~0xfLLU));
        //         }
        //     }
        // }

        switch ((controller.config.vendorID << 16) | controller.config.deviceID) {
            case (VENDOR_ID__INTEL << 16) | DEVICE_ID__82540EM_A: {
                // @TODO Pass NetworkController (it's globally available at the moment)
                bool res = i8254x_init();
                if (!res) {
                    *count = 0;
                    return;
                }
                devices[0] = &fake_device;
                *count = 1;
            } break;
            case (VENDOR_ID__REALTEK << 16) | DEVICE_ID__RTL8169: {
                // @TODO Pass NetworkController (it's globally available at the moment)
                bool res = rtl8169_init();
                if (!res) {
                    *count = 0;
                    return;
                }
                devices[0] = &fake_device;
                *count = 1;
            } break;
            default: {
                printf("[WARNING] NET_init: Could not find a supported Network Controller on the PCI bus.\n");
                *count = 0;
                break;
            }
        }
    }

    return;
}

void NET_cleanup() {
    // @TODO implement
}

void NET_device_info(NetDevice device, NET_DeviceInfo* info) {
    memcpy(info->mac, controller.mac_address, 6);
}

FN_NET_recv_packet g_recv_packet_callback;
void* g_recv_packet_callback_userData;

void NET_set_receive_callback(NetDevice device, FN_NET_recv_packet callback, void* user_data) {
    // @TODO Thread safety.
    g_recv_packet_callback = callback;
    g_recv_packet_callback_userData = user_data;
}

bool NET_poll_packet(NetDevice device, NET_Packet* packet) {
    // if (!packet) {
    //     printf("NET_poll_packet: PACKET IS NULL!\n");
    //     kernel_bug();
    //     return false;
    // 
    
    void* buffer;
    int size;

    switch (controller.config.deviceID) {
        case DEVICE_ID__82540EM_A: {
            i8254x_receive_packet(&buffer, &size);
        } break;
        case DEVICE_ID__RTL8169: {
            rtl8169_receive_packet(&buffer, &size);
        } break;
    }


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
    // if (!device) {
    //     printf("NET_send_packet: Device is NULL!\n");
    //     kernel_bug();
    //     return;
    // }
    switch (controller.config.deviceID) {
        case DEVICE_ID__82540EM_A: {
            int sent_bytes = i8254x_send_packet(buffer, size);
            if (sent_bytes < 0) {
                printf("Could not send packet, %d\n", sent_bytes);
            }
        } break;
        case DEVICE_ID__RTL8169: {
            int sent_bytes = rtl8169_send_packet(buffer, size);
            if (sent_bytes < 0) {
                printf("Could not send packet, %d\n", sent_bytes);
            }
        } break;
    }
}

void NET_send_arp(NetDevice net_device, uint32_t address) {
    
    static u8 g_packet_buffer[1024];

    // ARP packet
    int packet_size = sizeof(EtherFrame) + sizeof(ARP_Header_EthernetIPV4);
    EtherFrame* message_frame = (EtherFrame*)g_packet_buffer;
    memcpy(message_frame->destination, broadcast_mac, 6);
    memcpy(message_frame->source, current_mac, 6);
    message_frame->etherType = ETHER_ARP;
    ARP_Header_EthernetIPV4* message_arp = (ARP_Header_EthernetIPV4*)(g_packet_buffer + sizeof(EtherFrame));
    message_arp->hardware_type = ARP_ETHERNET;
    message_arp->protocol_type = ETHER_IPV4;
    message_arp->hardware_length = 6;
    message_arp->protocol_length = 4;
    message_arp->operation = ARP_REQUEST;
    memcpy(message_arp->sender_hw_address, current_mac, 6);
    memcpy(message_arp->sender_proto_address, &current_ip, 4);
    // memcpy(message_arp->target_hw_address, , 6); // MAC is what we're asking for. this field is unset
    memcpy(message_arp->target_proto_address, &address, 4);

    message_frame->etherType   = bswap16(message_frame->etherType);
    message_arp->hardware_type = bswap16(message_arp->hardware_type);
    message_arp->protocol_type = bswap16(message_arp->protocol_type);
    message_arp->operation     = bswap16(message_arp->operation);

    NET_send_packet(net_device, g_packet_buffer, packet_size);
}

void NET_send_dhcp_discover(NetDevice device) {
    
    int dhcpSize = sizeof(DHCP_Header) +8 +1; // +8 because of options, +1 because option end
    int udpSize = sizeof(UDP_Header) + dhcpSize;

    u8 message_buffer[sizeof(EtherFrame) + sizeof(IPV4_Header) + sizeof(UDP_Header) + sizeof(DHCP_Header) + 64] = {0};

    int packet_size = sizeof(EtherFrame) + sizeof(IPV4_Header) + udpSize;

    if (packet_size > sizeof(message_buffer)) {
        printf("NET: UDP packet data to big for static buffer, dropping\n");
        return;
    }

    EtherFrame* message_frame = (EtherFrame*)message_buffer;
    memcpy(message_frame->destination, broadcast_mac, 6);
    memcpy(message_frame->source, current_mac, 6);
    message_frame->etherType = ETHER_IPV4;
    IPV4_Header* message_ipv4 = (IPV4_Header*)(message_buffer + sizeof(EtherFrame));
    message_ipv4->headerLength = sizeof(IPV4_Header) / 4;
    message_ipv4->version = 4;
    message_ipv4->totalLength = sizeof(IPV4_Header) + udpSize;
    message_ipv4->identification = 0;
    message_ipv4->fragmentPart = IPV4_FLAG_DONT_FRAGMENT;
    message_ipv4->headerChecksum = 0;
    message_ipv4->timeToLive = 64;
    message_ipv4->protocol = IP_UDP;
    message_ipv4->sourceAddress = 0;
    message_ipv4->destinationAddress = 0xFFFFFFFF; // limited broadcast, can't use directed broadcast since we don't know subnet.

    message_frame->etherType = bswap16(message_frame->etherType);
    message_ipv4->totalLength = bswap16(message_ipv4->totalLength);
    message_ipv4->identification = bswap16(message_ipv4->identification);
    message_ipv4->fragmentPart = bswap16(message_ipv4->fragmentPart);

    message_ipv4->headerChecksum = bswap16(compute_internet_checksum(message_ipv4, sizeof(IPV4_Header)));

    UDP_Header* message_udp = (UDP_Header*)(message_buffer + sizeof(EtherFrame) + sizeof(IPV4_Header));
    message_udp->sourcePort = 68;
    message_udp->destinationPort = 67;
    message_udp->checksum = 0;
    message_udp->length = udpSize;

    message_udp->sourcePort = bswap16(message_udp->sourcePort);
    message_udp->destinationPort = bswap16(message_udp->destinationPort);
    message_udp->length = bswap16(message_udp->length);

    DHCP_Header* message_dhcp = (DHCP_Header*)((char*)message_udp + sizeof(UDP_Header));
    message_dhcp->op = 1;
    message_dhcp->htype = 1;
    message_dhcp->hlen = 6;
    message_dhcp->hops = 0;
    message_dhcp->xid = 0x3903F326;
    message_dhcp->secs = 0;
    message_dhcp->flags = 0;
    message_dhcp->ciaddr = 0;
    message_dhcp->yiaddr = 0;
    message_dhcp->siaddr = 0;
    memset(message_dhcp->chaddr, 0, 16);
    memcpy(message_dhcp->chaddr, current_mac, 6);
    memset(message_dhcp->zeros, 0, sizeof(message_dhcp->zeros));
    message_dhcp->magicCookie = DHCP_MAGIC_COOKIE;
    int opt_head = 0;
    message_dhcp->options[opt_head++] = DHCP_OPTION_TYPE;
    message_dhcp->options[opt_head++] = 1; // length of option
    message_dhcp->options[opt_head++] = DHCP_DISCOVER;
    message_dhcp->options[opt_head++] = DHCP_OPTION_PARAM_REQUEST_LIST;
    message_dhcp->options[opt_head++] = 3;
    message_dhcp->options[opt_head++] = DHCP_OPTION_SUBNET_MASK;
    message_dhcp->options[opt_head++] = DHCP_OPTION_ROUTER;
    message_dhcp->options[opt_head++] = DHCP_OPTION_DOMAIN_SERVER;
    message_dhcp->options[opt_head++] = 0xFF;

    message_dhcp->magicCookie = bswap32(message_dhcp->magicCookie);
    message_dhcp->xid = bswap32(message_dhcp->xid);

    UDP_Pseudo_Header pseudo = {0};
    pseudo.sourceAddress = message_ipv4->sourceAddress;
    pseudo.destinationAddress = message_ipv4->destinationAddress;
    pseudo.protocol = IP_UDP;
    pseudo.udpLength = bswap16(udpSize);

    message_udp->checksum = bswap16(~compute_internet_checksum(&pseudo, sizeof(UDP_Pseudo_Header)));
    message_udp->checksum = bswap16(compute_internet_checksum(message_udp, udpSize));

    NET_send_packet(device, message_buffer, packet_size);
}

void NET_send_dhcp_request(NetDevice device, u32 request_address, u32 dhcp_server) {
    
    int dhcpSize = sizeof(DHCP_Header) +15 +1; // +15 because of options, +1 because option end
    int udpSize = sizeof(UDP_Header) + dhcpSize;

    u8 message_buffer[sizeof(EtherFrame) + sizeof(IPV4_Header) + sizeof(UDP_Header) + sizeof(DHCP_Header) + 64] = {0};

    int packet_size = sizeof(EtherFrame) + sizeof(IPV4_Header) + udpSize;

    if (packet_size > sizeof(message_buffer)) {
        printf("NET: UDP packet data to big for static buffer, dropping\n");
        return;
    }

    EtherFrame* message_frame = (EtherFrame*)message_buffer;
    memcpy(message_frame->destination, broadcast_mac, 6);
    memcpy(message_frame->source, current_mac, 6);
    message_frame->etherType = ETHER_IPV4;
    IPV4_Header* message_ipv4 = (IPV4_Header*)(message_buffer + sizeof(EtherFrame));
    message_ipv4->headerLength = sizeof(IPV4_Header) / 4;
    message_ipv4->version = 4;
    message_ipv4->totalLength = sizeof(IPV4_Header) + udpSize;
    message_ipv4->identification = 0;
    message_ipv4->fragmentPart = IPV4_FLAG_DONT_FRAGMENT;
    message_ipv4->headerChecksum = 0;
    message_ipv4->timeToLive = 64;
    message_ipv4->protocol = IP_UDP;
    message_ipv4->sourceAddress = 0;
    message_ipv4->destinationAddress = 0xFFFFFFFF; // limited broadcast, can't use directed broadcast since we don't know subnet.

    message_frame->etherType = bswap16(message_frame->etherType);
    message_ipv4->totalLength = bswap16(message_ipv4->totalLength);
    message_ipv4->identification = bswap16(message_ipv4->identification);
    message_ipv4->fragmentPart = bswap16(message_ipv4->fragmentPart);

    message_ipv4->headerChecksum = bswap16(compute_internet_checksum(message_ipv4, sizeof(IPV4_Header)));

    UDP_Header* message_udp = (UDP_Header*)(message_buffer + sizeof(EtherFrame) + sizeof(IPV4_Header));
    message_udp->sourcePort = 68;
    message_udp->destinationPort = 67;
    message_udp->checksum = 0;
    message_udp->length = udpSize;

    message_udp->sourcePort = bswap16(message_udp->sourcePort);
    message_udp->destinationPort = bswap16(message_udp->destinationPort);
    message_udp->length = bswap16(message_udp->length);

    DHCP_Header* message_dhcp = (DHCP_Header*)((char*)message_udp + sizeof(UDP_Header));
    message_dhcp->op = 1;
    message_dhcp->htype = 1;
    message_dhcp->hlen = 6;
    message_dhcp->hops = 0;
    message_dhcp->xid = 0x3903F326;
    message_dhcp->secs = 0;
    message_dhcp->flags = 0;
    message_dhcp->ciaddr = 0;
    message_dhcp->yiaddr = 0;
    message_dhcp->siaddr = dhcp_server;
    memset(message_dhcp->chaddr, 0, 16);
    memcpy(message_dhcp->chaddr, current_mac, 6);
    memset(message_dhcp->zeros, 0, sizeof(message_dhcp->zeros));
    message_dhcp->magicCookie = DHCP_MAGIC_COOKIE;
    int opt_head = 0;
    message_dhcp->options[opt_head++] = DHCP_OPTION_TYPE;
    message_dhcp->options[opt_head++] = 1; // length of option
    message_dhcp->options[opt_head++] = DHCP_REQUEST;
    message_dhcp->options[opt_head++] = DHCP_OPTION_REQUEST_IP;
    message_dhcp->options[opt_head++] = 4;
    *(u32*)&message_dhcp->options[opt_head] = request_address;
    opt_head+=4;
    message_dhcp->options[opt_head++] = DHCP_OPTION_DHCP_SERVER;
    message_dhcp->options[opt_head++] = 4;
    *(u32*)&message_dhcp->options[opt_head] = dhcp_server;
    opt_head+=4;
    message_dhcp->options[opt_head++] = 0xFF;

    message_dhcp->magicCookie = bswap32(message_dhcp->magicCookie);
    message_dhcp->xid = bswap32(message_dhcp->xid);

    UDP_Pseudo_Header pseudo = {0};
    pseudo.sourceAddress = message_ipv4->sourceAddress;
    pseudo.destinationAddress = message_ipv4->destinationAddress;
    pseudo.protocol = IP_UDP;
    pseudo.udpLength = bswap16(udpSize);

    message_udp->checksum = bswap16(~compute_internet_checksum(&pseudo, sizeof(UDP_Pseudo_Header)));
    message_udp->checksum = bswap16(compute_internet_checksum(message_udp, udpSize));

    NET_send_packet(device, message_buffer, packet_size);
}

bool NET_handle_packet(NetDevice device, NET_Packet* packet) {
    
    // @NOCHECKIN We need to check that lengths specified in packet doesn't extend the length of the whole packet. Malicious or corrupt packet should be dropped if so.

    if (!packet->buffer || !packet->size)
        return true;

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
                // printf("ARP hw=%s proto=%s oper=%s senderMAC=%s senderIP=%s targetMAC=%s targetIP=%s\n",
                //     htype_str(arp_ipv4->hardware_type),
                //     ether_str(arp_ipv4->protocol_type),
                //     oper_str(arp_ipv4->operation),
                //     mac_str(arp_ipv4->sender_hw_address, buffer0),
                //     ipv4_str(arp_ipv4->sender_proto_address, buffer1),
                //     mac_str(arp_ipv4->target_hw_address, buffer2),
                //     ipv4_str(arp_ipv4->target_proto_address, buffer3)
                //     );


                if (arp_ipv4->operation == ARP_REQUEST && *(u32*)arp_ipv4->target_proto_address == current_ip) {
                    u8 message_buffer[sizeof(EtherFrame) + sizeof(ARP_Header_EthernetIPV4)] = {0};
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
                    return true;
                }
            } else {
                // printf("ARP hw=%s proto=%s oper=%s\n",
                //     htype_str(arp->hardware_type),
                //     ether_str(arp->protocol_type),
                //     oper_str(arp->operation)
                // );
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
            // printf("IP size=%d foffset=%d ttl=%d proto=%s chksum=%d (computed %d) src=%s dst=%s\n",
            //     ip->totalLength,
            //     ip->fragmentPart & IPV4_FRAGMENT_OFFSET_MASK,
            //     ip->timeToLive,
            //     ipproto_str(ip->protocol),
            //     ip->headerChecksum,
            //     computed_checksum,
            //     ipv4_int_str(ip->sourceAddress, buffer0),
            //     ipv4_int_str(ip->destinationAddress, buffer1)
            //     );
            
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

                        // printf("ICMP type=%d code=%d chksum=%d (computed %d) ident=%d seq=%d\n",
                        //     echo->type,
                        //     echo->code,
                        //     echo->checksum,
                        //     computed_checksum,
                        //     echo->identifier,
                        //     echo->sequence_number
                        //     );
                        
                        if (computed_checksum != 0) {
                            // Don't send anything back, checksum is bad (or my implementation is bad)
                            return true;
                        }

                        u8 message_buffer[sizeof(EtherFrame) + sizeof(IPV4_Header) + sizeof(ICMP_Header_Echo) + 256] = {0};

                        int packet_size = sizeof(EtherFrame) + sizeof(IPV4_Header) + icmp_size;

                        if (packet_size > sizeof(message_buffer)) {
                            printf("NET: ICMP packet payload to big for static buffer, dropping\n");
                            return true;
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
                        return true;
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
                    
                    DHCP_Header* dhcp = (DHCP_Header*)((char*)udp + sizeof(UDP_Header));
                    
                    if (udpSize >= sizeof(UDP_Header) + sizeof(DHCP_Header) && bswap32(dhcp->magicCookie) == DHCP_MAGIC_COOKIE) {
                        int opt_length = udp->length - sizeof(UDP_Header) + sizeof(DHCP_Header);
                        u8  msg_type = 0xFF;
                        u32 offered_address = dhcp->yiaddr;
                        u32 subnet_mask = 0;
                        u32 router = 0;
                        u32 lease = 0;
                        int head = 0;
                        while (head < opt_length) {
                            u8 opt = dhcp->options[head];
                            head++;
                            if (opt == 0xFF)
                                break;

                            u8 len = dhcp->options[head];
                            head++;
                            if (opt == DHCP_OPTION_TYPE) {
                                msg_type = dhcp->options[head];
                            } else if (opt == DHCP_OPTION_SUBNET_MASK) {
                                subnet_mask = *((u32*)&dhcp->options[head]);
                            } else if (opt == DHCP_OPTION_ROUTER) {
                                router = *((u32*)&dhcp->options[head]);
                            } else if (opt == DHCP_OPTION_ADDRESS_TIME) {
                                lease = *((u32*)&dhcp->options[head]);
                            } else if (opt == DHCP_OPTION_DOMAIN_SERVER) {
                                // @TODO What to do with domain server? do we always get 3 ip addresses?
                            }
                            head += len;
                        }

                        if (msg_type == DHCP_OFFER) {
                            printf("Recieved DHCP offer: %d.%d.%d.%d\n",
                                offered_address&0xFF,
                                (offered_address>>8)&0xFF,
                                (offered_address>>16)&0xFF,
                                (offered_address>>24)&0xFF);
                            current_ip = offered_address; // can't set this yet, we need to wait for ACK
                            NET_send_dhcp_request(device, offered_address, dhcp->siaddr);
                            return true;
                        } else  if (msg_type == DHCP_ACK) {
                            printf("DHCP ACK\n");
                            return true;
                        } else {
                            printf("Unhandled DHCP, type=%d\n", msg_type);
                        }

                        break;
                    }

                    int body_size = udpSize - sizeof(UDP_Header);
                    u8* body = (u8*)udp + sizeof(UDP_Header);

                    if (!memcmp(body, "ism", 3)) {
                        // some message from router?
                        return true;
                        break;
                    }

                    // printf("  UDP srcPort=%d dstPort=%d \n", udp->sourcePort, udp->destinationPort);

                    // int head = 0;
                    // printf(" %x: ", head);
                    // while (head < body_size) {
                    //     u8 byte = body[head];
                    //     head++;

                    //     printf("%x%x ", byte>>4, byte&0xF);
                    //     if (head % 8 == 7)
                    //         printf(" ");
                    //     if (head % 32 == 31)
                    //         printf("\n 0x%x: ", head);
                    // }
                    // printf("\n");
                    // printf("As text:\n");
                    // body[body_size] = 0; // ensure null termination
                    // printf("%s\n", body);
                    // printf("\n");

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

    return false;
}

