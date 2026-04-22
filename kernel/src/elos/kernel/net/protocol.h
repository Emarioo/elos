#pragma once

#include "elos/common/types.h"

/*
    Ethernet Frame
*/

enum _EtherType {
    ETHER_IPV4 = 0x800,
    ETHER_ARP  = 0x806,
    ETHER_IPV6 = 0x86DD,
};
typedef u16 EtherType;

#pragma pack(push, 1)
typedef struct EtherFrame {
    u8 destination[6];
    u8 source[6];
    EtherType etherType;
} EtherFrame;
#pragma pack(pop)


/*
    Address Resolution Protocol (ARP)
*/


enum _ARP_HardwareType {
    ARP_ETHERNET = 1,
};
typedef u16 ARP_HardwareType;

enum _ARP_Operation {
    ARP_REQUEST = 1,
    ARP_REPLY   = 2,
};
typedef u16 ARP_Operation;

#pragma pack(push, 1)
typedef struct ARP_Header {
    ARP_HardwareType hardware_type;
    EtherType protocol_type;
    u8 hardware_length;
    u8 protocol_length;
    ARP_Operation operation;
    u8 _payload[];
    // u8 sender_hw_address[6];
    // u8 sender_proto_address[4];
    // u8 target_hw_address[6];
    // u8 target_proto_address[4];
} ARP_Header;

typedef struct ARP_Header_EthernetIPV4 {
    ARP_HardwareType hardware_type;
    EtherType protocol_type;
    u8 hardware_length; // length in bytes (6 for ethernet)
    u8 protocol_length; // length in bytes (4 for ipv4)
    ARP_Operation operation;
    u8 sender_hw_address[6];
    u8 sender_proto_address[4];
    u8 target_hw_address[6];
    u8 target_proto_address[4];
} ARP_Header_EthernetIPV4;
#pragma pack(pop)


/*
    IP Packet
*/


enum _IP_Protocol {
    IP_ICMP = 1,
    IP_TCP = 6,
    IP_UDP = 17,
};
typedef u8 IP_Protocol;

#pragma pack(push, 1)
typedef struct IPV4_Header {
    u8 headerLength : 4; // each unit is worth 4 bytes
    u8 version : 4;
    u8 ecn : 2;
    u8 dscp : 6;
    u16 totalLength;
    u16 identification;
    u16 fragmentPart;
    u8 timeToLive;
    IP_Protocol protocol;
    u16 headerChecksum;
    u32 sourceAddress;
    u32 destinationAddress;
    u8 _options[];
} IPV4_Header;
#pragma pack(pop)
    
#define IPV4_FLAG_DONT_FRAGMENT   0x4000
#define IPV4_FLAG_MORE_FRAGMENTS  0x2000
#define IPV4_FRAGMENT_OFFSET_MASK 0x1FFF


/*
    Internet Control Message Protocol (ICMP)
*/


enum _ICMP_Type {
    ICMP_ECHO_REPLY = 0,
    ICMP_ECHO_REQUEST = 8,
};
typedef u8 ICMP_Type;

#pragma pack(push, 1)
typedef struct ICMP_Header {
    ICMP_Type type;
    u8 code;
    u16 checksum;
    u8 _data[];
} ICMP_Header;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct ICMP_Header_Echo {
    ICMP_Type type;
    u8 code;
    u16 checksum;
    u16 identifier;
    u8 sequence_number;
    u8 payload[];
} ICMP_Header_Echo;
#pragma pack(pop)

const char* htype_str(ARP_HardwareType type);
const char* oper_str(ARP_Operation type);
const char* ether_str(EtherType type);
const char* ipproto_str(IP_Protocol type);

const char* mac_str(u8 mac[6], char* buffer);
const char* ipv4_str(u8 address[4], char* buffer);
const char* ipv4_int_str(u32 address, char* buffer);

static inline u16 bswap16(u16 x) {
    return __builtin_bswap16(x);
}
static inline u32 bswap32(u32 x) {
    return __builtin_bswap32(x);
}
static inline u64 bswap64(u32 x) {
    return __builtin_bswap64(x);
}

u16 compute_internet_checksum(u8* buffer, int size);

u32 ipv4_from_str(const char* address);
