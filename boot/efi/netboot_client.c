#include "netboot/netboot.h"

#include "elos/common/string.h"

#include "elos/kernel/net/protocol.h"
#include "elos/network.h"

#include "efi.h"

#include "elos/common/intrinsics.h"

/*
    @TODO Tweak timeouts for DHCP, ARP, NetBoot requests.

    On QEMU we increase them a bit because emulation and wierdness.
    On Hardware we can assume local network latency is less than 10ms in worst case.
    Probably a lot less but maybe network card runs in default slow mode or something.
*/


uint8_t  my_mac_address[6];
uint32_t my_ip_address;

uint8_t  target_mac_address[6];
uint32_t target_ip_address;

int TARGET_PORT = NETBOOT_DEFAULT_PORT;
int SRC_PORT = NETBOOT_DEFAULT_PORT;

void KCON_printf(const char* format, ...);

#define printf(...) KCON_printf(__VA_ARGS__)


#define debug(...) KCON_printf(__VA_ARGS__)
// #define debug(...)

u32 ipv4_from_str(const char* address);

bool NETBOOT_query_mac(uint32_t address, uint8_t mac[6]);
bool NETBOOT_query_dhcp_ip(uint32_t* address);

extern EFI_SIMPLE_NETWORK_PROTOCOL* simple_network; // defined in main.c

bool NETBOOT_handle_base(char* buffer, int size);

NetBoot_Impl g_impl;
NetBoot_Config g_config;

uint64_t crude_measure();
uint64_t now_us();

// DELETE
bool NETBOOT_test_recv(uint32_t address, uint8_t mac[6]);

uint64_t tsc_per_ms;
uint64_t base_tsc;

bool NETBOOT_init(NetBoot_Impl* impl, NetBoot_Config* config) {
    tsc_per_ms = crude_measure();

    g_impl = *impl;
    g_config = *config;
    memcpy(my_mac_address, g_impl.mac, 6);

    bool yes = NETBOOT_query_dhcp_ip(&my_ip_address);
    if (!yes) {
        // If we get no DHCP response then we will use a hardcoded address for the 
        // machine we are booting on.
        // (with tap0 we have no DHCP server on the interface, with -net user qemu flag we do get DHCP, but no ARP i believe)
        my_ip_address = g_config.static_ip;
        char buffer0[30];
        printf("No response for DHCP, using static IP address: %s\n", ipv4_str((u8*)&my_ip_address, buffer0));
    }

    bool got_mac = false;
    for (int i=0;i<config->server_ips_len;i++) {
        target_ip_address = config->server_ips[i];
        bool yes = NETBOOT_query_mac(target_ip_address, target_mac_address);
        if (yes) {
            got_mac = true;
            break;
        }
    }

    // NETBOOT_test_recv(target_ip_address, target_mac_address);

    if (!got_mac) {
        printf("Did not receive ARP reply. IP doesn't exist or we need to wait longer.\n");
        return false;
    }
    return true;
}


static u8 g_recv_buffer[0x10000];
static u8 g_send_buffer[0x10000];


int send_packet(void* buffer, int size) {
    int bytes = g_impl.send(g_impl.device, buffer, size);
    return bytes;
}
int recv_packet(void* buffer, int* out_size) {
    int bytes = g_impl.recv(g_impl.device, buffer, out_size);

    if (bytes == 0) {
        *out_size = 0;
        return 0;
    }
    bool handled = NETBOOT_handle_base(buffer, bytes);
    if (handled) {
        *out_size = 0;
        return 0;
    }
    *out_size = bytes;
    return bytes;
}

void request_file(const char* path, uint64_t offset, uint64_t size);
void send_file_ack(uint8_t mac[6], uint32_t address, uint16_t port, uint64_t offset, uint16_t size);


void send_arp(uint32_t address) {

    // ARP packet
    int packet_size = sizeof(EtherFrame) + sizeof(ARP_Header_EthernetIPV4);
   
    construct_arp(g_send_buffer, &packet_size, g_impl.mac, my_ip_address, address);

    send_packet(g_send_buffer, packet_size);
}

void send_dhcp_discover() {
    
    int dhcpSize = sizeof(DHCP_Header) +8 +1; // +8 because of options, +1 because option end
    int udpSize = sizeof(UDP_Header) + dhcpSize;

    u8 message_buffer[sizeof(EtherFrame) + sizeof(IPV4_Header) + sizeof(UDP_Header) + sizeof(DHCP_Header) + 64] = {0};

    int packet_size = sizeof(EtherFrame) + sizeof(IPV4_Header) + udpSize;

    construct_dhcp_discover(message_buffer, &packet_size, g_impl.mac);

    send_packet(message_buffer, packet_size);
}


void send_dhcp_request(u32 request_address, u32 dhcp_server) {
    
    int dhcpSize = sizeof(DHCP_Header) +15 +1; // +15 because of options, +1 because option end
    int udpSize = sizeof(UDP_Header) + dhcpSize;

    u8 message_buffer[sizeof(EtherFrame) + sizeof(IPV4_Header) + sizeof(UDP_Header) + sizeof(DHCP_Header) + 64] = {0};
    int packet_size = sizeof(EtherFrame) + sizeof(IPV4_Header) + udpSize;

    construct_dhcp_request(message_buffer, &packet_size, g_impl.mac, request_address, dhcp_server);

    send_packet(message_buffer, packet_size);
}

bool NETBOOT_query_dhcp_ip(uint32_t* address) {

    u32 received_address = 0;

    send_dhcp_discover();

    uint64_t startTime = now_us();
    uint64_t timeout_us = 100*1000;

    while (1) {
        int buffer_size = sizeof(g_recv_buffer);
        bool res = recv_packet(g_recv_buffer, &buffer_size);
        if (!res) {
            uint64_t now = now_us();
            if (now - startTime >= timeout_us) {
                return false;
            }
            pause();
            // We want to send DHCP again. It's UDP after all.
            //   If we didn't receive offer then we should send discover.
            //   If we didnt receive ACK then we should send request.
            continue;
        }
        
        EtherFrame* frame = (EtherFrame*)g_recv_buffer;

        frame->etherType = bswap16(frame->etherType);

        // printf("Ether typ %d\n", frame->etherType);

        if (frame->etherType == ETHER_IPV4) {
            IPV4_Header* ip = (IPV4_Header*)((char*)g_recv_buffer + sizeof(EtherFrame));

            // header length specifies number of 32-bit words (at least 5)
            u16 computed_checksum = compute_internet_checksum(ip, ip->headerLength * 4);
 
            ip->headerChecksum = bswap16(ip->headerChecksum);
            ip->totalLength = bswap16(ip->totalLength);
            ip->identification = bswap16(ip->identification);
            ip->fragmentPart = bswap16(ip->fragmentPart);
            
            int ipHeaderSize = ip->headerLength * 4;

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
                        // @TODO What to do with domain servers?
                    }
                    head += len;
                }

                if (msg_type == DHCP_OFFER) {
                    printf("Recieved DHCP offer: %d.%d.%d.%d\n",
                        offered_address&0xFF,
                        (offered_address>>8)&0xFF,
                        (offered_address>>16)&0xFF,
                        (offered_address>>24)&0xFF);
                    received_address = offered_address; // can't set this yet, we need to wait for ACK
                    send_dhcp_request(offered_address, dhcp->siaddr);
                } else  if (msg_type == DHCP_ACK) {
                    printf("DHCP ACK\n");
                    break;
                } else {
                    printf("Unhandled DHCP, type=%d\n", msg_type);
                }
            }
        }
    }

    if (received_address != 0) {
        *address = received_address;
        return true;
    }

    return false;
}

bool NETBOOT_query_mac(uint32_t address, uint8_t mac[6]) {

    send_arp(address);
    
    uint64_t startTime       = now_us();
    uint64_t lastSentTime    = now_us();
    uint64_t timeout_us      = 1000*1000;
    uint64_t timeout_send_us = 400*1000;
    // int resends = 0;
    while (1) {
        int buffer_size = sizeof(g_recv_buffer);
        bool res = recv_packet(g_recv_buffer, &buffer_size);
        if (!res) {
            uint64_t now = now_us();
            if (now - lastSentTime >= timeout_send_us) {
                lastSentTime = now;
                // printf("Send arp\n");
                // send_arp(address);
                // resends++;
            }
            if (now - startTime >= timeout_us) {
                break;
            }
            continue;
        }
        
        EtherFrame* frame = (EtherFrame*)g_recv_buffer;

        frame->etherType = bswap16(frame->etherType);

        // printf("Ether typ %d\n", frame->etherType);

        if (frame->etherType == ETHER_ARP) {
            ARP_Header* arp = (ARP_Header*)((char*)g_recv_buffer + sizeof(EtherFrame));

            arp->hardware_type = bswap16(arp->hardware_type);
            arp->protocol_type = bswap16(arp->protocol_type);
            arp->operation     = bswap16(arp->operation);

            // printf("ARP oper %d\n", arp->operation);
            
            if (arp->hardware_type == ARP_ETHERNET && arp->protocol_type == ETHER_IPV4) {
                ARP_Header_EthernetIPV4* arp_ipv4 = (ARP_Header_EthernetIPV4*)arp;
                uint64_t now = now_us();
                memcpy(mac, arp_ipv4->sender_hw_address, 6);
                char buffer0[30];
                printf("Got MAC from ARP, %s, %d us\n", mac_str(mac,buffer0), (now - startTime));
                // sucess
                return true;
            }
        }
    }
    // printf("Resends %d\n", resends);

    return false;
}



bool NETBOOT_handle_base(char* buffer, int size) {
    EtherFrame* frame = (EtherFrame*)g_recv_buffer;

    int etherType = bswap16(frame->etherType);

    if (etherType == ETHER_ARP) {
        ARP_Header* arp = (ARP_Header*)((char*)g_recv_buffer + sizeof(EtherFrame));

        int hardware_type = bswap16(arp->hardware_type);
        int protocol_type = bswap16(arp->protocol_type);
        int operation     = bswap16(arp->operation);

        // printf("ARP op: %d\n", operation);
        
        if (hardware_type == ARP_ETHERNET && protocol_type == ETHER_IPV4 && operation == ARP_REQUEST) {
            ARP_Header_EthernetIPV4* arp_ipv4 = (ARP_Header_EthernetIPV4*)arp;
            // memcpy(mac, arp_ipv4->sender_hw_address, 6);
            // printf("Got MAC from ARP\n");

            int packet_size = sizeof(EtherFrame) + sizeof(ARP_Header_EthernetIPV4);
            construct_arp_reply(g_send_buffer, &packet_size, g_impl.mac, my_ip_address, arp_ipv4->sender_hw_address, *(u32*)&arp_ipv4->sender_proto_address);

            debug("Handled ARP REQUEST\n");
            send_packet(g_send_buffer, packet_size);
            return true;
        }
    }
    return false;
}

extern EFI_SYSTEM_TABLE *ST;


uint64_t crude_measure() {
    EFI_STATUS status;
    uint64_t start_tsc, end_tsc;
    EFI_EVENT timer_event;

    uint64_t wait_time_us = 100*1000; // 10 ms

    status = ST->BootServices->CreateEvent(EFI_EVENT_TIMER, TPL_APPLICATION, NULL, NULL, &timer_event);
    if (EFI_ERROR(status)) {
        printf("CreateEvent failed, %d\n", status);
        return 1;
    }

    status = ST->BootServices->RaiseTPL(TPL_APPLICATION);
    if (EFI_ERROR(status)) {
        printf("RaiseTPL failed, %d\n", status);
        return 1;
    }

    start_tsc = rdtsc();

    status = ST->BootServices->SetTimer(timer_event, TimerRelative, wait_time_us*10); // 100ns units
    if (EFI_ERROR(status)) {
        printf("SetTimer failed, %d\n", status);
        return 1;
    }

    UINTN index;
    status = ST->BootServices->WaitForEvent(1, &timer_event, &index);
    if (EFI_ERROR(status)) {
        printf("WaitForEvent failed, %d\n", status);
        return 1;
    }

    end_tsc = rdtsc();

    uint64_t diff_tsc = end_tsc - start_tsc;
    uint64_t per_ms = (1000 * diff_tsc) / wait_time_us;
    // printf("DIFFS %d M %d clock/ms\n", diff_tsc, per_ms);

    return per_ms;
}


uint64_t now_us() {
    if (tsc_per_ms == 0) {
        tsc_per_ms = crude_measure();
    }
    if (base_tsc == 0) {
        base_tsc = rdtsc();
    }

    return (1000*(rdtsc() - base_tsc)) / tsc_per_ms;
}

// Room for 10.9375 megabytes. ((1024*8 * 1400)/1024/1024)
#define BITMAP_SIZE 1024

u64 g_bitmap[BITMAP_SIZE/8];
u32 g_finishedChunk;
u32 g_totalChunks;

void bitmap_set_totalChunks(u32 totalChunks) {
    u32 maxChunks = BITMAP_SIZE*8;
    if (totalChunks > maxChunks) {
        printf("netboot: bitmap can store %d chunks, but file needs %d (%d MB)\n", maxChunks, totalChunks, (totalChunks * NETBOOT_CHUNK_SIZE) / 0x100000);
        while (1) pause();
    }
    g_totalChunks = totalChunks;
}
void bitmap_reset(u32 totalChunks) {
    memset(g_bitmap, 0, sizeof(g_bitmap));
    g_finishedChunk = 0;
    bitmap_set_totalChunks(totalChunks);
}
void bitmap_set(u32 chunk) {
    int quad = chunk / 64;
    int bit = chunk % 64;
    g_bitmap[quad] |= (u64)1 << bit;
}
bool bitmap_finished() {
    while (g_finishedChunk < g_totalChunks) {
        u32 chunk = g_finishedChunk;
        int quad = chunk / 64;
        int bit = chunk % 64;
        if (!(g_bitmap[quad] & ((u64)1 << bit))) {
            return false;
        }
        g_finishedChunk++;
    }
    return true;
}

void bitmap_dump(void)
{
    printf("Bitmap (%u chunks, finished=%u)\n",
        g_totalChunks,
        g_finishedChunk);

    for (u32 chunk = 0; chunk < g_totalChunks; chunk++)
    {
        bool set =
            g_bitmap[chunk / 64] &
            ((u64)1 << (chunk % 64));

        printf("%c",set ? '1' : '0');

        if ((chunk % 8) == 7)
            printf(" ");

        if ((chunk % 64) == 63)
            printf("\n");
    }

    printf("\n");
}

void bitmap_dump_missing(void)
{
    int missing = 0;

    printf("Missing chunks:\n");

    for (u32 chunk = 0; chunk < g_totalChunks; chunk++)
    {
        bool set =
            g_bitmap[chunk / 64] &
            ((u64)1 << (chunk % 64));

        if (!set)
        {
            printf("%u ", chunk);
            missing++;
        }
    }

    printf("\nMissing count = %d\n", missing);
}


// offset and size are optional
int NETBOOT_request_file(const char* path, uint64_t offset, uint64_t size, void* buffer) {
    printf("Requesting %s\n", path);


    bitmap_reset((size + NETBOOT_CHUNK_SIZE-1) / NETBOOT_CHUNK_SIZE);

    request_file(path, offset, size);

    uint64_t start_us = now_us();
    uint64_t timeoutStart_us = start_us;

    uint64_t timeoutValue = 1000 * 1000; // You want something higher on QEMU.
    // int limit = limit_cap;
    while (1) {
        int buffer_size = sizeof(g_recv_buffer);
        bool res = recv_packet(g_recv_buffer, &buffer_size);
        if (!res) {
            // @TODO If we have received no SEND_FILE packets then
            //   our request was probably dropped.
            //   We should request it again.

            // @TODO If we haven't received all data then and no more SEND_FILE packets
            //   are arriving then we should let server know what we are missing.
            uint64_t now = now_us();
            // printf("wait %d...\n", (now - timeoutStart_us) / 1000);
            if (now - timeoutStart_us > timeoutValue) {
                break;
            }
            pause();
            // printf("limit %d recvbytes=%d\n", limit, received_bytes);
            continue;
        }
        
        EtherFrame* frame = (EtherFrame*)g_recv_buffer;

        frame->etherType = bswap16(frame->etherType);

        if (frame->etherType != ETHER_IPV4) {
            // printf("Not IPv4 %d\n", frame->etherType);
            continue;
        }
    
        IPV4_Header* ipv4 = (IPV4_Header*)((char*)g_recv_buffer + sizeof(EtherFrame));
        if (ipv4->protocol != IP_UDP) {
            // printf("Not UDP %d\n", ipv4->protocol);
            continue;
        }
        UDP_Header* udp = (UDP_Header*)((char*)g_recv_buffer + sizeof(EtherFrame) + sizeof(IPV4_Header));
        udp->destinationPort = bswap16(udp->destinationPort);
        udp->sourcePort = bswap16(udp->sourcePort);
        udp->length = bswap16(udp->length);
        udp->checksum = bswap16(udp->checksum);

        // printf("UDP src=%d dst=%d len=%d chk=%d\n", udp->sourcePort, udp->destinationPort, udp->length, udp->checksum);

        if (udp->destinationPort != SRC_PORT) {
            // printf("Wrong port, %d\n", udp->destinationPort);
            continue;
        }
        
        NetBoot_Header* net = (NetBoot_Header*)((char*)udp + sizeof(UDP_Header));
        if (memcmp(net->magic, NETBOOT_MAGIC, 4) || net->version != 1) {
            printf("Not bootmagic, ver=%d magic=\"%c%c%c%c\"\n", net->version, net->magic[0], net->magic[1], net->magic[2], net->magic[3]);
            continue;
        }
        
        // printf("NAS\n");
        if (net->type != NETBOOT_SEND_FILE) {
            printf("NetBOOT Type is not SEND FILE, %d\n", net->type);
            continue;
        }
        NetBoot_Send_File* sendf = (NetBoot_Send_File*)net;

        int buffer_offset = sendf->offset - offset;
        if (buffer_offset < 0 || buffer_offset + sendf->size > size) {
            printf("Invalid offsets %d %d\n", sendf->offset, sendf->size);
            // Invalid size/offsets, malicious packet?
            continue;
        }
        
        // printf("Memcpy %x, %d, %d\n", buffer, buffer_offset, sendf->size);
        // printf("CHILL\n");
        memcpy((char*)buffer + buffer_offset, sendf->payload, sendf->size);

        // debug("Recv file size, recbytes=%d off=%d recvsize=%d total=%d foff=%d\n", received_bytes, buffer_offset, sendf->size, sendf->totalFileSize, sendf->offset);
        timeoutStart_us = now_us();

        send_file_ack(frame->source, ipv4->sourceAddress, udp->sourcePort, sendf->offset, sendf->size);
        
        // We get file size from data payload message.
        // So we update it.
        bitmap_set_totalChunks((sendf->totalFileSize + NETBOOT_CHUNK_SIZE-1) / NETBOOT_CHUNK_SIZE);
        bitmap_set(sendf->offset / NETBOOT_CHUNK_SIZE);

        // Our last ACK may have dropped and not arrived at server. Server will
        // keep resending packets. Which we won't answer too.
        // Server will timeout the session eventually.

        if (bitmap_finished()) {
            printf("Finished %s\n", path);
            return sendf->totalFileSize;
        }
    }

    // bitmap_dump_missing();

    // bitmap_dump();
    
    printf("No response on NETBOOT file request? (or incomplete response)\n");

    return 0;
}


void request_file(const char* path, uint64_t offset, uint64_t size) {
    
    int message_udpSize = sizeof(UDP_Header) + sizeof(NetBoot_Request_File) + strlen(path) + 1;

    int packet_size = sizeof(EtherFrame) + sizeof(IPV4_Header) + message_udpSize;

    if (packet_size > sizeof(g_send_buffer)) {
        printf("NET: UDP packet data to big for static buffer, dropping\n");
        // return 0;
    }

    EtherFrame* message_frame = (EtherFrame*)g_send_buffer;
    memcpy(message_frame->destination, target_mac_address, 6);
    memcpy(message_frame->source, my_mac_address, 6);
    message_frame->etherType = ETHER_IPV4;
    
    message_frame->etherType = bswap16(message_frame->etherType);

    IPV4_Header* message_ipv4 = (IPV4_Header*)(g_send_buffer + sizeof(EtherFrame));
    message_ipv4->headerLength = sizeof(IPV4_Header) / 4;
    message_ipv4->version = 4;
    message_ipv4->totalLength = sizeof(IPV4_Header) + message_udpSize;
    message_ipv4->identification = 0;
    message_ipv4->fragmentPart = IPV4_FLAG_DONT_FRAGMENT;
    message_ipv4->headerChecksum = 0;
    message_ipv4->timeToLive = 64;
    message_ipv4->protocol = IP_UDP;
    memcpy(&message_ipv4->sourceAddress, &my_ip_address, 4);
    memcpy(&message_ipv4->destinationAddress, &target_ip_address, 4);

    message_ipv4->totalLength = bswap16(message_ipv4->totalLength);
    message_ipv4->identification = bswap16(message_ipv4->identification);
    message_ipv4->fragmentPart = bswap16(message_ipv4->fragmentPart);

    message_ipv4->headerChecksum = bswap16(compute_internet_checksum(message_ipv4, sizeof(IPV4_Header)));

    UDP_Header* message_udp = (UDP_Header*)(g_send_buffer + sizeof(EtherFrame) + sizeof(IPV4_Header));
    message_udp->sourcePort = SRC_PORT;
    message_udp->destinationPort = g_config.port;
    message_udp->checksum = 0;
    message_udp->length = message_udpSize;

    message_udp->sourcePort = bswap16(message_udp->sourcePort);
    message_udp->destinationPort = bswap16(message_udp->destinationPort);
    message_udp->length = bswap16(message_udp->length);

    NetBoot_Request_File* req = (NetBoot_Request_File*)((char*)message_udp + sizeof(UDP_Header));
    memcpy(req->header.magic, NETBOOT_MAGIC, 4);
    req->header.version = 1;
    req->header.type = NETBOOT_REQUEST_FILE;
    req->offset = offset;
    req->size = size;
    req->filePath_len = strlen(path);
    memcpy(req->filePath, path, req->filePath_len);
    req->filePath[req->filePath_len] = '\0';

    // @TODO Checksum. Need pseduo header for ipv4
    UDP_Pseudo_Header pseudo = {0};
    pseudo.sourceAddress = message_ipv4->sourceAddress;
    pseudo.destinationAddress = message_ipv4->destinationAddress;
    pseudo.protocol = IP_UDP;
    pseudo.udpLength = bswap16(message_udpSize);

    message_udp->checksum = bswap16(~compute_internet_checksum(&pseudo, sizeof(UDP_Pseudo_Header)));

    message_udp->checksum = bswap16(compute_internet_checksum(message_udp, message_udpSize));

    send_packet(g_send_buffer, packet_size);
}

void send_file_ack(uint8_t mac[6], uint32_t address, uint16_t port, uint64_t offset, uint16_t size) {
    int message_udpSize = sizeof(UDP_Header) + sizeof(NetBoot_Send_File_Ack);

    int packet_size = sizeof(EtherFrame) + sizeof(IPV4_Header) + message_udpSize;

    if (packet_size > sizeof(g_send_buffer)) {
        printf("NET: UDP packet data to big for static buffer, dropping\n");
        // return 0;
    }

    EtherFrame* message_frame = (EtherFrame*)g_send_buffer;
    memcpy(message_frame->destination, mac, 6);
    memcpy(message_frame->source, my_mac_address, 6);
    message_frame->etherType = ETHER_IPV4;
    
    IPV4_Header* message_ipv4 = (IPV4_Header*)(g_send_buffer + sizeof(EtherFrame));
    message_ipv4->headerLength = sizeof(IPV4_Header) / 4;
    message_ipv4->version = 4;
    message_ipv4->totalLength = sizeof(IPV4_Header) + message_udpSize;
    message_ipv4->identification = 0;
    message_ipv4->fragmentPart = IPV4_FLAG_DONT_FRAGMENT;
    message_ipv4->headerChecksum = 0;
    message_ipv4->timeToLive = 64;
    message_ipv4->protocol = IP_UDP;
    memcpy(&message_ipv4->sourceAddress, &my_ip_address, 4);
    memcpy(&message_ipv4->destinationAddress, &address, 4);

    message_frame->etherType = bswap16(message_frame->etherType);
    message_ipv4->totalLength = bswap16(message_ipv4->totalLength);
    message_ipv4->identification = bswap16(message_ipv4->identification);
    message_ipv4->fragmentPart = bswap16(message_ipv4->fragmentPart);

    message_ipv4->headerChecksum = bswap16(compute_internet_checksum(message_ipv4, sizeof(IPV4_Header)));

    UDP_Header* message_udp = (UDP_Header*)(g_send_buffer + sizeof(EtherFrame) + sizeof(IPV4_Header));
    message_udp->sourcePort = SRC_PORT;
    message_udp->destinationPort = port;
    message_udp->checksum = 0;
    message_udp->length = message_udpSize;

    message_udp->sourcePort = bswap16(message_udp->sourcePort);
    message_udp->destinationPort = bswap16(message_udp->destinationPort);
    message_udp->length = bswap16(message_udp->length);

    NetBoot_Send_File_Ack* req = (NetBoot_Send_File_Ack*)((char*)message_udp + sizeof(UDP_Header));
    memcpy(req->header.magic, NETBOOT_MAGIC, 4);
    req->header.version = 1;
    req->header.type = NETBOOT_SEND_FILE_ACK;
    req->offset = offset;
    req->size = size;

    // @TODO Checksum. Need pseduo header for ipv4
    UDP_Pseudo_Header pseudo = {0};
    pseudo.sourceAddress = message_ipv4->sourceAddress;
    pseudo.destinationAddress = message_ipv4->destinationAddress;
    pseudo.protocol = IP_UDP;
    pseudo.udpLength = bswap16(message_udpSize);

    message_udp->checksum = bswap16(~compute_internet_checksum(&pseudo, sizeof(UDP_Pseudo_Header)));

    message_udp->checksum = bswap16(compute_internet_checksum(message_udp, message_udpSize));

    send_packet(g_send_buffer, packet_size);

    // debug("Sent ack off=%d size=%d\n", (int)offset, (int)size);
}
