#include "elos/kernel/net/protocol.h"

#include "elos/common/string.h"

const char* htype_str(ARP_HardwareType type) {
    static char buffer[256];
    switch(type) {
        case ARP_ETHERNET: return "ETHERNET";
        default: break;
    }
    snprintf(buffer, sizeof(buffer), "%u", type);
    return buffer;
}

const char* oper_str(ARP_Operation type) {
    static char buffer[256];
    switch(type) {
        case ARP_REQUEST: return "REQUEST";
        case ARP_REPLY: return "REPLY";
        default: break;
    }
    snprintf(buffer, sizeof(buffer), "%u", type);
    return buffer;
}

const char* ether_str(EtherType type) {
    static char buffer[256];
    switch(type) {
        case ETHER_IPV4: return "IPV4";
        case ETHER_ARP: return "Arp";
        case ETHER_IPV6: return "IPV6";
        default: break;
    }
    snprintf(buffer, sizeof(buffer), "%u", type);
    return buffer;
}
const char* ipproto_str(IP_Protocol type) {
    static char buffer[256];
    switch(type) {
        case IP_ICMP: return "ICMP";
        case IP_TCP: return "TCP";
        case IP_UDP: return "UDP";
        default: break;
    }
    snprintf(buffer, sizeof(buffer), "%u", type);
    return buffer;
}

const char* icmptype_str(ICMP_Type type) {
    static char buffer[256];
    switch(type) {
        case ICMP_ECHO_REQUEST: return "ECHO REQUEST";
        case ICMP_ECHO_REPLY: return "ECHO REPLY";
        default: break;
    }
    snprintf(buffer, sizeof(buffer), "%u", type);
    return buffer;
}

const char* mac_str(u8 mac[6], char* buffer) {
    snprintf(buffer, 20, "%x%x:%x%x:%x%x:%x%x:%x%x:%x%x", 
        mac[0] >> 4, mac[0] & 0xF,
        mac[1] >> 4, mac[1] & 0xF,
        mac[2] >> 4, mac[2] & 0xF,
        mac[3] >> 4, mac[3] & 0xF,
        mac[4] >> 4, mac[4] & 0xF,
        mac[5] >> 4, mac[5] & 0xF
    );
    return buffer;
}
const char* ipv4_str(u8 address[4], char* buffer) {
    snprintf(buffer, 20, "%u.%u.%u.%u", 
        address[0], address[1], address[2], address[3]
    );
    return buffer;
}
const char* ipv4_int_str(u32 address, char* buffer) {
    snprintf(buffer, 20, "%u.%u.%u.%u", 
        address & 0xFF, (address >> 8) & 0xFF, (address >> 16) & 0xFF, address >> 24
    );
    return buffer;
}

u32 ipv4_from_str(const char* address) {
    char* string = (char*)address;
    u32 num;
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

uint8_t broadcast_mac[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

void construct_arp_reply(u8* message_buffer, int* buffer_len, u8 my_mac[6], uint32_t my_address, u8 target_mac[6], uint32_t target_address) {

    // ARP packet
    int packet_size = sizeof(EtherFrame) + sizeof(ARP_Header_EthernetIPV4);

    *buffer_len = packet_size;
    EtherFrame* message_frame = (EtherFrame*)message_buffer;
    memcpy(message_frame->destination, broadcast_mac, 6);
    memcpy(message_frame->source, my_mac, 6);
    message_frame->etherType = ETHER_ARP;

    ARP_Header_EthernetIPV4* message_arp = (ARP_Header_EthernetIPV4*)(message_buffer + sizeof(EtherFrame));
    message_arp->hardware_type = ARP_ETHERNET;
    message_arp->protocol_type = ETHER_IPV4;
    message_arp->hardware_length = 6;
    message_arp->protocol_length = 4;
    message_arp->operation = ARP_REPLY;
    memcpy(message_arp->sender_hw_address, my_mac, 6);
    memcpy(message_arp->sender_proto_address, &my_address, 4);
    memset(message_arp->sender_proto_address, 0, 4);
    memcpy(message_arp->target_hw_address, target_mac, 6);
    memcpy(message_arp->target_proto_address, &target_address, 4);

    message_frame->etherType   = bswap16(message_frame->etherType);
    message_arp->hardware_type = bswap16(message_arp->hardware_type);
    message_arp->protocol_type = bswap16(message_arp->protocol_type);
    message_arp->operation     = bswap16(message_arp->operation);
}

void construct_arp(u8* message_buffer, int* buffer_len, u8 mac[6], uint32_t address, uint32_t target_address) {
    // ARP packet
    int packet_size = sizeof(EtherFrame) + sizeof(ARP_Header_EthernetIPV4);
    *buffer_len = packet_size;

    EtherFrame* message_frame = (EtherFrame*)message_buffer;
    memcpy(message_frame->destination, broadcast_mac, 6);
    memcpy(message_frame->source, mac, 6);
    message_frame->etherType = ETHER_ARP;
    ARP_Header_EthernetIPV4* message_arp = (ARP_Header_EthernetIPV4*)(message_buffer + sizeof(EtherFrame));
    message_arp->hardware_type = ARP_ETHERNET;
    message_arp->protocol_type = ETHER_IPV4;
    message_arp->hardware_length = 6;
    message_arp->protocol_length = 4;
    message_arp->operation = ARP_REQUEST;
    memcpy(message_arp->sender_hw_address, mac, 6);
    memcpy(message_arp->sender_proto_address, &address, 4);
    memset(message_arp->target_hw_address, 0, 6); // MAC is what we're asking for. this field is unset
    memcpy(message_arp->target_proto_address, &target_address, 4);

    message_frame->etherType   = bswap16(message_frame->etherType);
    message_arp->hardware_type = bswap16(message_arp->hardware_type);
    message_arp->protocol_type = bswap16(message_arp->protocol_type);
    message_arp->operation     = bswap16(message_arp->operation);
}

void construct_dhcp_discover(u8* message_buffer, int* buffer_len, u8 mac[6]) {

    int dhcpSize = sizeof(DHCP_Header) +8 +1; // +8 because of options, +1 because option end
    int udpSize = sizeof(UDP_Header) + dhcpSize;

    // u8 message_buffer[sizeof(EtherFrame) + sizeof(IPV4_Header) + sizeof(UDP_Header) + sizeof(DHCP_Header) + 64] = {0};

    int packet_size = sizeof(EtherFrame) + sizeof(IPV4_Header) + udpSize;

    // if (packet_size > *buffer_len) {
    //     // printf("construct_dhcp_request: UDP packet data to big for static buffer, dropping\n");
    //     return;
    // }
    *buffer_len = packet_size;
    u8 broadcast_mac[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

    EtherFrame* message_frame = (EtherFrame*)message_buffer;
    memcpy(message_frame->destination, broadcast_mac, 6);
    memcpy(message_frame->source, mac, 6);
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
    memcpy(message_dhcp->chaddr, mac, 6);
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
}

void construct_dhcp_request(u8* message_buffer, int* buffer_len, u8 mac[6], u32 request_address, u32 dhcp_server) {
    
    int dhcpSize = sizeof(DHCP_Header) +15 +1; // +15 because of options, +1 because option end
    int udpSize = sizeof(UDP_Header) + dhcpSize;

    int packet_size = sizeof(EtherFrame) + sizeof(IPV4_Header) + udpSize;
    // if (packet_size > *buffer_len) {
    //     printf("construct_dhcp_request: UDP packet data to big for static buffer, dropping\n");
    //     return;
    // }
    *buffer_len = packet_size;
    u8 broadcast_mac[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

    EtherFrame* message_frame = (EtherFrame*)message_buffer;
    memcpy(message_frame->destination, broadcast_mac, 6);
    memcpy(message_frame->source, mac, 6);
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
    memcpy(message_dhcp->chaddr, mac, 6);
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
}