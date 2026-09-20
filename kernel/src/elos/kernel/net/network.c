/*

    @TODO Complete overhaul on networking. I think it's very easy to send malicious ARP or other types of packets
       for spoofing and so on. We need to check and test these things properly.

    @TODO Local routing through 127.0.0.1

*/


#include "elos/network.h"

#include "elos/kernel/net/i82574.h"
#include "elos/kernel/net/rtl8169.h"

#include "elos/kernel/net/protocol.h"

#include "elos/kernel_console.h"

#include "elos/common/string.h"

#include "elos/physical_memory.h"

#include "elos/kernel/net/net_internal.h"

#include "elos/common/intrinsics.h"

#include "elos/cpu.h"



#define printf(...) KCON_printf(__VA_ARGS__)



typedef struct ScanInfo ScanInfo;
struct ScanInfo {
    NET_Device** devices;
    int maxCount;
    int count;
};




NET_Device g_devices[MAX_NET_DEVICES];
int        g_devices_len;
int        g_devices_max = MAX_NET_DEVICES;




#define MAX_NET_HANDLES 100

u32 g_netHandles_max = MAX_NET_HANDLES;
NET_Handle g_netHandles[MAX_NET_HANDLES];
volatile u32 g_net_lock;






NET_Device* reserve_net_device() {
    NET_Device* device = &g_devices[g_devices_len];
    device->present = true;
    g_devices_len++;
    return device;
}

NetworkController controller;


bool find_device(PCI_Scanner* scanner, PCI_ConfigSpace* config) {
    bool returnValue = false;
    ScanInfo* scanInfo = scanner->user_data;

    if (config->classCode != PCI_CLASSCODE__NETWORK_CONTROLLER) {
        return false;
    }
    
    NET_Device* device = NULL;

    if (config->vendorID == VENDOR_ID__INTEL && config->deviceID == DEVICE_ID__82574L) {
        device = reserve_net_device();
        device->config = *config;

        returnValue = i82574_init(device);

        // @TODO unreserve device if it fails

    } else if (config->vendorID == VENDOR_ID__REALTEK && config->deviceID == DEVICE_ID__RTL8169) {
        
        // @TODO Fix this up
        controller.found = true;
        controller.config = *config;

        bool res = rtl8169_init();
        
        controller.found = res;

        return res;

    } else {
        // printf("[INFO] NET_init: Searching PCI for network card, found device id 0x%x (vendor 0x%x), but card is not supported.\n", config->deviceID, config->vendorID);
    }

    if (device) {
        // @TODO DHCP to get gateway, netmask, address.
        // @TODO Automatic Private IP Addressing if DHCP is unavailable.
        //   We currently use fixed address.

        device->info.ipv4_address     = ipv4_from_str("169.254.0.1");
        device->info.ipv4_subnet_mask = ipv4_from_str("255.255.0.0");
        device->info.ipv4_gateway     = 0; // no gateway
    }

    return returnValue;
}

void NET_scan_devices(NET_Device* devices[], int* count) {
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

    ScanInfo scanInfo = {
        .devices = devices,
        .count = 0,
        .maxCount = *count,
    };

    PCI_Scanner scanner = { .func = find_device, .user_data = &scanInfo };
    pci_scan_buses(&scanner);

    *count = scanInfo.count;
}

void NET_cleanup() {
    // @TODO implement
}

void NET_device_info(NET_Device* device, NET_DeviceInfo* info) {
    memcpy(info->mac, controller.mac_address, 6);
}

// FN_NET_recv_packet g_recv_packet_callback;
// void* g_recv_packet_callback_userData;

// void NET_set_receive_callback(NET_Device* device, FN_NET_recv_packet callback, void* user_data) {
//     // @TODO Thread safety.
//     g_recv_packet_callback = callback;
//     g_recv_packet_callback_userData = user_data;
// }

bool NET_poll_packet(NET_Device* device, NET_Packet* packet) {
    // if (!packet) {
    //     printf("NET_poll_packet: PACKET IS NULL!\n");
    //     kernel_bug();
    //     return false;
    // 
    
    void* buffer;
    u32 size;

    switch (device->config.deviceID) {
        case DEVICE_ID__82574L: {
            i82574_receive_packet(&buffer, &size);
        } break;
        case DEVICE_ID__RTL8169: {
            rtl8169_receive_packet(&buffer, &size);
        } break;
        default: {
            printf("%s: Unhandled vendor/device 0x%x/0x%x", __func__, device->config.vendorID, device->config.deviceID);
        } break;
    }


    if (!buffer || !size)
        return false;

    packet->buffer = buffer;
    packet->size = size;
    return true;
}
void NET_free_packet(NET_Packet* packet) {
    PMEM_free(packet->buffer);
    memset(packet, 0, sizeof(*packet));
}

bool NET_send_packet(NET_Device* device, const void* buffer, int size) {
    bool returnValue = false;
    // if (!device) {
    //     printf("NET_send_packet: Device is NULL!\n");
    //     kernel_bug();
    //     return;
    // }
    switch (device->config.deviceID) {
        case DEVICE_ID__82574L: {
            int sent_bytes = i82574_send_packet(buffer, size);
            if (sent_bytes < 0) {
                printf("Could not send packet, %d\n", sent_bytes);
            } else {
                returnValue = true;
            }
        } break;
        case DEVICE_ID__RTL8169: {
            int sent_bytes = rtl8169_send_packet(buffer, size);
            if (sent_bytes < 0) {
                printf("Could not send packet, %d\n", sent_bytes);
            } else {
                returnValue = true;
            }
        } break;
        default: {
            printf("%s: Unhandled vendor/device 0x%x/0x%x", __func__, device->config.vendorID, device->config.deviceID);
        } break;
    }
    return returnValue;
}


bool net_handle_packet(NET_Device* device, NET_Packet* packet) {
    bool consumedPacket = false;

    // @NOCHECKIN We need to check that lengths specified in packet doesn't extend the length of the whole packet. Malicious or corrupt packet should be dropped if so.

    KERNEL_PANIC(packet->buffer && packet->size, "WHY EMPTY?");

    // printf("HANDLE %d\n", packet->size);

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


                if (arp_ipv4->operation == ARP_REQUEST && *(u32*)arp_ipv4->target_proto_address == device->info.ipv4_address) {
                    u8 message_buffer[sizeof(EtherFrame) + sizeof(ARP_Header_EthernetIPV4)] = {0};
                    EtherFrame* message_frame = (EtherFrame*)message_buffer;
                    memcpy(message_frame->destination, arp_ipv4->sender_hw_address, 6);
                    memcpy(message_frame->source, device->info.mac, 6);
                    message_frame->etherType = ETHER_ARP;
                    ARP_Header_EthernetIPV4* message_arp = (ARP_Header_EthernetIPV4*)(message_buffer + sizeof(EtherFrame));
                    message_arp->hardware_type = ARP_ETHERNET;
                    message_arp->protocol_type = ETHER_IPV4;
                    message_arp->hardware_length = 6;
                    message_arp->protocol_length = 4;
                    message_arp->operation = ARP_REPLY;
                    memcpy(message_arp->sender_hw_address, device->info.mac, 6);
                    memcpy(message_arp->sender_proto_address, &device->info.ipv4_address, 4);
                    memcpy(message_arp->target_hw_address, arp_ipv4->sender_hw_address, 6);
                    memcpy(message_arp->target_proto_address, arp_ipv4->sender_proto_address, 4);

                    message_frame->etherType   = bswap16(message_frame->etherType);
                    message_arp->hardware_type = bswap16(message_arp->hardware_type);
                    message_arp->protocol_type = bswap16(message_arp->protocol_type);
                    message_arp->operation     = bswap16(message_arp->operation);

                    NET_send_packet(device, message_buffer, sizeof(message_buffer));
                    NET_free_packet(packet);
                    consumedPacket = true;
                } else if (arp_ipv4->operation == ARP_REPLY) {
                    arp_update_table(*(u32*)&arp_ipv4->sender_proto_address, device, arp_ipv4->sender_hw_address);
                    NET_free_packet(packet);
                    consumedPacket = true;
                }
            } else {
                printf("ARP hw=%s proto=%s oper=%s (cannot handle)\n",
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
                            printf("NET: ICMP bad checksum %x\n", computed_checksum);
                            // Don't send anything back, checksum is bad (or my implementation is bad)
                            NET_free_packet(packet);
                            consumedPacket = true;
                            goto exit;
                        }

                        u8 message_buffer[sizeof(EtherFrame) + sizeof(IPV4_Header) + sizeof(ICMP_Header_Echo) + 256] = {0};

                        int packet_size = sizeof(EtherFrame) + sizeof(IPV4_Header) + icmp_size;

                        if (packet_size > sizeof(message_buffer)) {
                            printf("NET: ICMP packet payload to big for static buffer, dropping\n");
                            NET_free_packet(packet);
                            consumedPacket = true;
                            goto exit;
                        }

                        EtherFrame* message_frame = (EtherFrame*)message_buffer;
                        memcpy(message_frame->destination, frame->source, 6);
                        memcpy(message_frame->source, device->info.mac, 6);
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
                        memcpy(&message_ipv4->sourceAddress, &device->info.ipv4_address, 4);
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
                        NET_free_packet(packet);
                        consumedPacket = true;
                        goto exit;
                    } else {
                        // @TODO Handle ICMP destination unreachable?
                        //    linux tap sends it to QEMU if we send a packet
                        //    to an address/port that isn't open.
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
                            if (opt == 0xFF) {
                                break;
                            }

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
                            device->info.ipv4_address = offered_address; // can't set this yet, we need to wait for ACK
                            NET_send_dhcp_request(device, offered_address, dhcp->siaddr);
                            NET_free_packet(packet);
                            consumedPacket = true;
                        } else  if (msg_type == DHCP_ACK) {
                            printf("DHCP ACK\n");
                            NET_free_packet(packet);
                            consumedPacket = true;
                        } else {
                            printf("Unhandled DHCP, type=%d\n", msg_type);
                        }
                        goto exit;
                    }

                    int body_size = udpSize - sizeof(UDP_Header);
                    u8* body = (u8*)udp + sizeof(UDP_Header);


                    LOCK_INT(&g_net_lock);

                    NET_Handle* foundHandle = NULL;
                    for (int i=0;i<g_netHandles_max;i++) {
                        NET_Handle* handle = &g_netHandles[i];
                        if (!handle->used) {
                            continue;
                        }

                        // printf("CMP %d %d : %x %x\n", handle->address.udp_tcp4.port, udp->destinationPort, handle->address.udp_tcp4.address, ip->destinationAddress);

                        if (handle->address.udp_tcp4.port == udp->destinationPort && (handle->address.udp_tcp4.address == 0 || handle->address.udp_tcp4.address == ip->destinationAddress)) {
                            foundHandle = handle;
                            break;
                        }
                    }

                    if (!foundHandle) {
                        UNLOCK_INT(&g_net_lock);
                        goto exit;
                    }
                    
                    if (foundHandle->address.protocol != ELOS_NET_PROTO_UDP_IPV4) {
                        UNLOCK_INT(&g_net_lock);
                        goto exit;
                    }


                    NET_PortMessage* portMessage = PMEM_alloc(sizeof(NET_PortMessage));
                    memset(portMessage, 0, sizeof(*portMessage));

                    // foundHandle->packetVersion++;

                    portMessage->rawPacket          = *packet;
                    portMessage->sourceAddress      = ip->sourceAddress;
                    portMessage->destinationAddress = ip->destinationAddress;
                    portMessage->sourcePort         = udp->sourcePort;
                    portMessage->destinationPort    = udp->destinationPort;
                    portMessage->data               = body;
                    portMessage->data_len           = body_size;

                    portMessage->nextMessage        = foundHandle->nextMessage;
                    foundHandle->nextMessage        = portMessage; // Set in handle last when message is complete

                    // foundHandle->packetVersion++;

                    UNLOCK_INT(&g_net_lock);

                    // if (!memcmp(body, "ism", 3)) {
                    //     // My router sends this message now and then.
                    //     goto exit;
                    // }

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

exit:
    return consumedPacket;
}





NET_Handle* NET_open(const ELOS_Net_Address* address) {
    NET_Handle* returnValue = NULL;
    LOCK_INT(&g_net_lock);

    char netaddr_buffer[64];

    // printf("NET_open: %p\n", net_address_str(*address, netaddr_buffer));

    bool portCollision = false;
    NET_Handle* foundHandle = NULL;
    for (int i=0;i<g_netHandles_max;i++) {
        NET_Handle* handle = &g_netHandles[i];

        if (handle->address.udp_tcp4.port == address->udp_tcp4.port) {
            portCollision = true;
        }

        if (!handle->used && !foundHandle) {
            foundHandle = handle;
            break;
        }
    }

    if (portCollision) {
        return NULL;
    }

    if (!foundHandle) {
        goto exit;
    }

    foundHandle->used = true;
    foundHandle->address = *address;
    
    returnValue = foundHandle;

exit:
    UNLOCK_INT(&g_net_lock);

    return returnValue;
}




void NET_close(NET_Handle* handle) {
    if (!handle) {
        return;
    }
    
    // printf("NET_close: %p\n", handle);

    LOCK_INT(&g_net_lock);

    // @TODO Free resources.
    handle->used = false;
    
    UNLOCK_INT(&g_net_lock);
}


ELOS_Error NET_write(NET_Handle* handle, const ELOS_Net_Address* address, const void* data, u32 size) {
    ELOS_Error returnValue = ELOS_ERR_UNKNOWN;
    // printf("NET_write: %p %d\n", handle, size);

    NET_Device* device;
    u8  mac[6];
    u32 ip_address;
    u16 dst_port;
    u16 src_port;

    // @TODO Validate parameters? Valid device for raw packets for example?
    //    Or does the caller do that (async handling code).

    // Save to stack in case caller i cheeky and changes address on another thread.
    // If they do we may still have an incomplete address.
    ELOS_Net_Address savedAddress = *address;

    if (savedAddress.protocol == ELOS_NET_PROTO_RAW) {
        device = savedAddress.raw.device;
    } else {
        src_port = handle->address.udp_tcp4.port;
        bool didResolve = fetch_mac_from_address(savedAddress.udp_tcp4.address, &device, mac);
        if (!didResolve) {
            return ELOS_ERR_NOT_FOUND;
        }
        ip_address = savedAddress.udp_tcp4.address;
        dst_port = savedAddress.udp_tcp4.port;
    }


    switch (address->protocol) {
        case ELOS_NET_PROTO_RAW: {
            static char macbuff[32];
            // printf("NET_write: raw, mac %s\n", mac_str(mac, macbuff));

            bool yes = NET_send_packet(device, data, size);
            if (!yes) {
                returnValue = ELOS_ERR_UNKNOWN;
                goto exit;
            }
        } break;
        case ELOS_NET_PROTO_UDP_IPV4: {
            static char macbuff[32];

            // printf("NET_write: udp, mac %s\n", mac_str(mac, macbuff));

            bool yes = NET_send_udp(device, mac, ip_address, src_port, dst_port, data, size);
            if (!yes) {
                returnValue = ELOS_ERR_UNKNOWN;
                goto exit;
            }

            returnValue = ELOS_OK;
        } break;
        default: {
            returnValue = ELOS_ERR_INVALID_PARAM;
        } break;
    }

exit:
    return returnValue;
}

ELOS_Error NET_read(NET_Handle* handle, ELOS_Net_Address* address, void* buffer, u32* bufferSize, u64 timeout_ns) {
    ELOS_Error returnValue = ELOS_ERR_UNKNOWN;

    u64 tickStart = CPU_ticks();
    u64 tps = CPU_ticks_per_second();
    u64 timeout_tick = tickStart + (timeout_ns/10 * tps/10) / 10000000;

    // @TODO Better sleep and rescheduling.
    while (true) {

        if (handle->nextMessage) {

            LOCK_INT(&g_net_lock);
            
            if (handle->nextMessage) {
                NET_PortMessage* message = handle->nextMessage;

                address->protocol = handle->address.protocol;
                address->udp_tcp4.address = message->sourceAddress;
                address->udp_tcp4.port = message->sourcePort;

                if (*bufferSize >= message->data_len) {
                    memcpy(buffer, message->data, message->data_len);
                    *bufferSize = message->data_len;
                    handle->nextMessage = message->nextMessage;
                    PMEM_free(message);
                    UNLOCK_INT(&g_net_lock);
                    returnValue = ELOS_OK;
                    break;
                } else {
                    UNLOCK_INT(&g_net_lock);
                    returnValue = ELOS_ERR_BUFFER_SIZE_TOO_SMALL;
                    break;
                }
            }

            UNLOCK_INT(&g_net_lock);
        }

        u64 tickNow = CPU_ticks();
        if (tickNow > timeout_tick) {
            // printf("NET_read timeout\n");
            returnValue = ELOS_ERR_TIMEOUT;
            break;
        }

        pause();
    }


    return returnValue;
}








