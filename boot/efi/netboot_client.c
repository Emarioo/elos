#include "elos/netboot.h"

#include "elos/common/string.h"

#include "elos/kernel/net/protocol.h"

#include "efi.h"

#include "elos/common/intrinsics.h"

uint8_t  my_mac_address[6];
uint32_t my_ip_address;

uint8_t  target_mac_address[6];
uint32_t target_ip_address;

int TARGET_PORT = NETBOOT_DEFAULT_PORT;
int SRC_PORT = NETBOOT_DEFAULT_PORT;

uint8_t broadcast_mac[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

void printf(const char* format, ...);

// #define debug(...) printf(__VA_ARGS__)
#define debug(...)

u32 ipv4_from_str(const char* address);

void NETBOOT_query_mac(uint32_t address, uint8_t mac[6]);

extern EFI_SIMPLE_NETWORK_PROTOCOL* simple_network; // defined in main.c

bool NETBOOT_handle_base(char* buffer, int size);

bool recv_packet(void* buffer, int* size) {
    EFI_STATUS status;
    UINTN buffer_size = *size;
    UINT32 interruptMask;
    void* tx_buf;
    
    status = simple_network->GetStatus(simple_network, &interruptMask, &tx_buf);

    status = simple_network->Receive(simple_network, 0, &buffer_size, buffer, NULL, NULL, NULL);
    if (status == EFI_NOT_READY) {
        // printf("RECV NOT ready\r\n");
        *size = 0;
        return false;
    }
    if (EFI_ERROR(status)) {
        printf("Cannot receive network, %d, buffer size %d\r\n", status, buffer_size);
        *size = 0;
        return false;
    }
    status = simple_network->GetStatus(simple_network, &interruptMask, &tx_buf);

    EFI_NETWORK_STATISTICS stats = {};
    UINTN stat_size = sizeof(EFI_NETWORK_STATISTICS);
    status = simple_network->Statistics(simple_network, 0, &stat_size, &stats);
    if (EFI_ERROR(status)) {
        printf("Statistics error, %d\r\n", status);
    }

    debug("Got %d bytes, good=%d total=%x drop=%x totalb=%d statS=%d\r\n", buffer_size, stats.RxGoodFrames, stats.RxTotalFrames, stats.RxDroppedFrames, stats.RxTotalBytes, stat_size);

    bool handled = NETBOOT_handle_base(buffer, buffer_size);
    if (handled)
        return false;

    *size = buffer_size;
    return true;
}
void send_packet(const void* buffer, int size) {
    EFI_STATUS status;
    
    status = simple_network->Transmit(simple_network, 0, size, (void*)buffer, NULL, NULL, NULL);
    if (EFI_ERROR(status)) {
        printf("Could not transmit, %d\r\n", status);
        return;
    }

    UINT32 interruptMask;
    void* tx_buf;
    while (1) {
        status = simple_network->GetStatus(simple_network, &interruptMask, &tx_buf);
        pause();
        if (tx_buf == buffer)
            break;
    }
}


void NETBOOT_init() {
    memcpy(my_mac_address, simple_network->Mode->CurrentAddress.Addr, 6);

    my_ip_address = ipv4_from_str("192.168.100.54"); // @TODO Don't harcode IP

    target_ip_address = ipv4_from_str("192.168.100.50"); // @TODO Don't harcode target IP
    
    NETBOOT_query_mac(target_ip_address, target_mac_address);
}


static char g_recv_buffer[0x10000];
static char g_send_buffer[0x10000];


void request_file(const char* path, uint64_t offset, uint64_t size);
void send_file_ack(uint8_t mac[6], uint32_t address, uint16_t port, uint64_t offset, uint16_t size);

void send_arp(uint32_t address, uint8_t mac[6]) {

    // ARP packet
    int packet_size = sizeof(EtherFrame) + sizeof(ARP_Header_EthernetIPV4);
    EtherFrame* message_frame = (EtherFrame*)g_send_buffer;
    memcpy(message_frame->destination, broadcast_mac, 6);
    memcpy(message_frame->source, my_mac_address, 6);
    message_frame->etherType = ETHER_ARP;
    ARP_Header_EthernetIPV4* message_arp = (ARP_Header_EthernetIPV4*)(g_send_buffer + sizeof(EtherFrame));
    message_arp->hardware_type = ARP_ETHERNET;
    message_arp->protocol_type = ETHER_IPV4;
    message_arp->hardware_length = 6;
    message_arp->protocol_length = 4;
    message_arp->operation = ARP_REQUEST;
    memcpy(message_arp->sender_hw_address, my_mac_address, 6);
    memcpy(message_arp->sender_proto_address, &my_ip_address, 4);
    // memcpy(message_arp->target_hw_address, , 6); // MAC is what we're asking for. this field is unset
    memcpy(message_arp->target_proto_address, &address, 4);

    message_frame->etherType   = bswap16(message_frame->etherType);
    message_arp->hardware_type = bswap16(message_arp->hardware_type);
    message_arp->protocol_type = bswap16(message_arp->protocol_type);
    message_arp->operation     = bswap16(message_arp->operation);

    send_packet(g_send_buffer, packet_size);
}

void NETBOOT_query_mac(uint32_t address, uint8_t mac[6]) {

    send_arp(address, mac);

    int limit = 100000;
    while (limit) {
        limit--;
        int buffer_size = sizeof(g_recv_buffer);
        bool res = recv_packet(g_recv_buffer, &buffer_size);
        if (!res) {
            if (limit % 10000 == 0) {
                send_arp(address, mac);
            }
            continue;
        }
        
        EtherFrame* frame = (EtherFrame*)g_recv_buffer;

        frame->etherType = bswap16(frame->etherType);

        // printf("Ether typ %d\r\n", frame->etherType);

        if (frame->etherType == ETHER_ARP) {
            ARP_Header* arp = (ARP_Header*)((char*)g_recv_buffer + sizeof(EtherFrame));

            arp->hardware_type = bswap16(arp->hardware_type);
            arp->protocol_type = bswap16(arp->protocol_type);
            arp->operation     = bswap16(arp->operation);

            // printf("ARP oper %d\r\n", arp->operation);
            
            if (arp->hardware_type == ARP_ETHERNET && arp->protocol_type == ETHER_IPV4) {
                ARP_Header_EthernetIPV4* arp_ipv4 = (ARP_Header_EthernetIPV4*)arp;
                
                memcpy(mac, arp_ipv4->sender_hw_address, 6);
                printf("Got MAC from ARP\r\n");
                // sucess
                return;
            }
        }
    }

    printf("Did not receive ARP reply. IP doesn't exist or we need to wait longer.\r\n");
}

bool NETBOOT_handle_base(char* buffer, int size) {

    EtherFrame* frame = (EtherFrame*)g_recv_buffer;

    int etherType = bswap16(frame->etherType);

    if (etherType == ETHER_ARP) {
        ARP_Header* arp = (ARP_Header*)((char*)g_recv_buffer + sizeof(EtherFrame));

        int hardware_type = bswap16(arp->hardware_type);
        int protocol_type = bswap16(arp->protocol_type);
        int operation     = bswap16(arp->operation);
        
        if (hardware_type == ARP_ETHERNET && protocol_type == ETHER_IPV4 && operation == ARP_REQUEST) {
            ARP_Header_EthernetIPV4* arp_ipv4 = (ARP_Header_EthernetIPV4*)arp;
            // memcpy(mac, arp_ipv4->sender_hw_address, 6);
            // printf("Got MAC from ARP\r\n");

            // ARP packet
            int packet_size = sizeof(EtherFrame) + sizeof(ARP_Header_EthernetIPV4);
            EtherFrame* message_frame = (EtherFrame*)g_send_buffer;
            memcpy(message_frame->destination, broadcast_mac, 6);
            memcpy(message_frame->source, my_mac_address, 6);
            message_frame->etherType = ETHER_ARP;

            ARP_Header_EthernetIPV4* message_arp = (ARP_Header_EthernetIPV4*)(g_send_buffer + sizeof(EtherFrame));
            message_arp->hardware_type = ARP_ETHERNET;
            message_arp->protocol_type = ETHER_IPV4;
            message_arp->hardware_length = 6;
            message_arp->protocol_length = 4;
            message_arp->operation = ARP_REPLY;
            memcpy(message_arp->sender_hw_address, my_mac_address, 6);
            memcpy(message_arp->sender_proto_address, &my_ip_address, 4);
            memcpy(message_arp->target_hw_address, arp_ipv4->sender_hw_address, 6);
            memcpy(message_arp->target_proto_address, arp_ipv4->sender_proto_address, 4);

            message_frame->etherType   = bswap16(message_frame->etherType);
            message_arp->hardware_type = bswap16(message_arp->hardware_type);
            message_arp->protocol_type = bswap16(message_arp->protocol_type);
            message_arp->operation     = bswap16(message_arp->operation);

            debug("Handled ARP REQUEST\r\n");
            send_packet(g_send_buffer, packet_size);
            return true;
        }
    }
    return false;
}

extern EFI_SYSTEM_TABLE *ST;


uint64_t time_to_us(const EFI_TIME tim) {
    // This is flawed
    return (uint64_t) tim.Year * 12 * 31 * 24 * 60 * 60 * 1000000 +
        (uint64_t) tim.Month * 31 * 24 * 60 * 60 * 1000000 +
    (uint64_t) tim.Day * 24 * 60 * 60 * 1000000 +
    (uint64_t) tim.Hour * 60 * 60 * 1000000 +
    (uint64_t) tim.Minute * 60 * 1000000 +
    (uint64_t) tim.Second * 1000000 +
    (uint64_t) tim.Nanosecond / 1000;
}

uint64_t crude_measure() {
    EFI_STATUS status;
    EFI_TIME start_time, end_time;
    uint64_t start_tsc, end_tsc;

    status = ST ->RuntimeServices->GetTime(&start_time, NULL);
    if (EFI_ERROR(status)) {
        printf("GetTime err %d\r\n", status);
    }
    start_tsc = rtdsc();

    while (1) {
        end_tsc = rtdsc();
        status = ST ->RuntimeServices->GetTime(&end_time, NULL);
        
        if (EFI_ERROR(status))
            break;

        if (end_time.Second == (start_time.Second + 1) % 60) {
            break;
        }
    }

    int64_t diff_us = 1000000;
    int64_t diff_tsc = end_tsc - start_tsc;
    int64_t per_sec = (1000000*diff_tsc) / diff_us;
    // printf("DIFFS %d M %d %d M/sec\r\n", diff_tsc / 1000000, diff_us, per_sec/1000000);

    return per_sec;
}

uint64_t tsc_per_second;
uint64_t base_tsc;

uint64_t now_us() {
    if (tsc_per_second == 0) {
        tsc_per_second = crude_measure();
    }
    if (base_tsc == 0) {
        base_tsc = rtdsc();
    }

    return (1000000*(rtdsc() - base_tsc)) / tsc_per_second;
}


// offset and size are optional
int NETBOOT_request_file(const char* path, uint64_t offset, uint64_t size, void* buffer) {
    tsc_per_second = crude_measure();

    printf("Requesting %s\r\n", path);

    request_file(path, offset, size);

    int received_bytes = 0;

    uint64_t start_us = now_us();
    uint64_t timeoutStart_us = now_us();

    uint64_t timeoutValue = 1800 * 1000; // 800 ms
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
            // printf("wait %d...\r\n", (now - timeoutStart_us) / 1000);
            if (now - timeoutStart_us > timeoutValue) {
                break;
            }
            pause();
            // printf("limit %d recvbytes=%d\r\n", limit, received_bytes);
            continue;
        }
        
        EtherFrame* frame = (EtherFrame*)g_recv_buffer;

        frame->etherType = bswap16(frame->etherType);

        if (frame->etherType != ETHER_IPV4) {
            continue;
        }
    
        IPV4_Header* ipv4 = (IPV4_Header*)((char*)g_recv_buffer + sizeof(EtherFrame));
        if (ipv4->protocol != IP_UDP) {
            // printf("Not UDP\r\n");
            continue;
        }
        UDP_Header* udp = (UDP_Header*)((char*)g_recv_buffer + sizeof(EtherFrame) + sizeof(IPV4_Header));
        udp->destinationPort = bswap16(udp->destinationPort);
        udp->sourcePort = bswap16(udp->sourcePort);
        udp->length = bswap16(udp->length);
        udp->checksum = bswap16(udp->checksum);

        // printf("UDP src=%d dst=%d len=%d chk=%d\r\n", udp->sourcePort, udp->destinationPort, udp->length, udp->checksum);

        if (udp->destinationPort != SRC_PORT) {
            // printf("Wrong port, %d\r\n", udp->destinationPort);
            continue;
        }
        NetBoot_Header* net = (NetBoot_Header*)((char*)udp + sizeof(UDP_Header));
        if (memcmp(net->magic, NETBOOT_MAGIC, 4) || net->version != 1) {
            printf("Not bootmagic, ver=%d magic=\"%c%c%c%c\"\r\n", net->version, net->magic[0], net->magic[1], net->magic[2], net->magic[3]);
            continue;
        }
        if (net->type != NETBOOT_SEND_FILE) {
            printf("NetBOOT Type is not SEND FILE, %d\r\n", net->type);
            continue;
        }
        NetBoot_Send_File* sendf = (NetBoot_Send_File*)net;

        int buffer_offset = sendf->offset - offset;
        if (buffer_offset < 0 || buffer_offset + sendf->size > size) {
            printf("Invalid offsets %d %d\r\n", sendf->offset, sendf->size);
            // Invalid size/offsets, malicious packet?
            continue;
        }
        
        // printf("Memcpy %x, %d, %d\r\n", buffer, buffer_offset, sendf->size);

        memcpy(buffer + buffer_offset, sendf->payload, sendf->size);

        received_bytes += sendf->size;


        debug("Recv file size, recbytes=%d off=%d recvsize=%d total=%d foff=%d\r\n", received_bytes, buffer_offset, sendf->size, sendf->totalFileSize, sendf->offset);
        timeoutStart_us = now_us();

        send_file_ack(frame->source, ipv4->sourceAddress, udp->sourcePort, sendf->offset, sendf->size);

        // Our last ACK may have dropped and not arrived at server. Server will
        // keep resending packets. Which we won't answer too.

        if (size == received_bytes || received_bytes == sendf->totalFileSize - offset) {
            return received_bytes;
        } else if (received_bytes > size) {
            printf("Recevied too many bytes, %d > %d\r\n", received_bytes, size);
            break;
        }
    }

    printf("No response on NETBOOT file request? (or incomplete response)\r\n");

    return 0;
}


void request_file(const char* path, uint64_t offset, uint64_t size) {
    
    int message_udpSize = sizeof(UDP_Header) + sizeof(NetBoot_Request_File) + strlen(path) + 1;

    int packet_size = sizeof(EtherFrame) + sizeof(IPV4_Header) + message_udpSize;

    if (packet_size > sizeof(g_send_buffer)) {
        printf("NET: UDP packet data to big for static buffer, dropping\r\n");
        // return 0;
    }

    EtherFrame* message_frame = (EtherFrame*)g_send_buffer;
    memcpy(message_frame->destination, target_mac_address, 6);
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
    memcpy(&message_ipv4->destinationAddress, &target_ip_address, 4);

    message_frame->etherType = bswap16(message_frame->etherType);
    message_ipv4->totalLength = bswap16(message_ipv4->totalLength);
    message_ipv4->identification = bswap16(message_ipv4->identification);
    message_ipv4->fragmentPart = bswap16(message_ipv4->fragmentPart);

    message_ipv4->headerChecksum = bswap16(compute_internet_checksum(message_ipv4, sizeof(IPV4_Header)));

    UDP_Header* message_udp = (UDP_Header*)(g_send_buffer + sizeof(EtherFrame) + sizeof(IPV4_Header));
    message_udp->sourcePort = SRC_PORT;
    message_udp->destinationPort = TARGET_PORT;
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
    // UDP_Pseudo_Header pseudo = {};
    // pseudo.sourceAddress = message_ipv4->sourceAddress;
    // pseudo.destinationAddress = message_ipv4->destinationAddress;
    // pseudo.protocol = IP_UDP;
    // pseudo.udpLength = bswap16(message_udpSize);

    // message_udp->checksum = bswap16(~compute_internet_checksum(&pseudo, sizeof(UDP_Pseudo_Header)));

    // message_udp->checksum = bswap16(compute_internet_checksum(message_udp, message_udpSize));

    send_packet(g_send_buffer, packet_size);
}

void send_file_ack(uint8_t mac[6], uint32_t address, uint16_t port, uint64_t offset, uint16_t size) {
    int message_udpSize = sizeof(UDP_Header) + sizeof(NetBoot_Send_File_Ack);

    int packet_size = sizeof(EtherFrame) + sizeof(IPV4_Header) + message_udpSize;

    if (packet_size > sizeof(g_send_buffer)) {
        printf("NET: UDP packet data to big for static buffer, dropping\r\n");
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
    // UDP_Pseudo_Header pseudo = {};
    // pseudo.sourceAddress = message_ipv4->sourceAddress;
    // pseudo.destinationAddress = message_ipv4->destinationAddress;
    // pseudo.protocol = IP_UDP;
    // pseudo.udpLength = bswap16(message_udpSize);

    // message_udp->checksum = bswap16(~compute_internet_checksum(&pseudo, sizeof(UDP_Pseudo_Header)));

    // message_udp->checksum = bswap16(compute_internet_checksum(message_udp, message_udpSize));

    send_packet(g_send_buffer, packet_size);

    debug("Sent ack off=%d size=%d\r\n", (int)offset, (int)size);
}

uint32_t ipv4_from_str(const char* address) {
    char* string = (char*)address;
    uint32_t num;
    num  = (u32)strtol(string  , &string, 10);
    num |= (u32)strtol(string+1, &string, 10) << 8;
    num |= (u32)strtol(string+1, &string, 10) << 16;
    num |= (u32)strtol(string+1, &string, 10) << 24;
    return num;
}

u16 compute_internet_checksum(void* header, int size) {
    u32 acc = 0;
    int half_words = (size + 1) / 2;

    u16* data = (u16*)header;
    for (int i=0;i<half_words;i++) {
        acc += (u16)bswap16(data[i]);
    }

    // Add carries to 16-bit part
    while (acc >> 16)
        acc = (acc & 0xFFFF) + (acc >> 16);

    return ~acc;
}
