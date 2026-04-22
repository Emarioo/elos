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



u16 compute_internet_checksum(u8* header, int size) {
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
