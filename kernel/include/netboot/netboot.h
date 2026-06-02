/*
    Custom Network Boot Protocol

    File paths are relative to NetBoot server's base directory for served files.

    API is flawed and wierd. sorry.

    Protocol (PC is the one booting):
        PC -> ARP, "Who has address 192.168.0.100"
        PC <- ARP, "3f:82:ab:fa:23:98 has address 192.168.0.100"
        PC -> 192.168.0.100 UDP/NetBoot, "Request file list"
        PC <- 192.168.0.100 UDP/NetBoot, "Send file list"
        PC -> 192.168.0.100 UDP/NetBoot, "Request file '/KERNEL.IMG'"
        PC <- 192.168.0.100 UDP/NetBoot, "Send file '/KERNEL.IMG' off:0,    size:1024, totalFileSize: 2244"
        PC <- 192.168.0.100 UDP/NetBoot, "Send file '/KERNEL.IMG' off:1024, size:1024, totalFileSize: 2244"
        PC <- 192.168.0.100 UDP/NetBoot, "Send file '/KERNEL.IMG' off:2048, size:196,  totalFileSize: 2244"

    UDP is not reliable. NetBoot client has to resend requests if no response. If still no response then NetBoot has failed.

    If a send file request does not reach PC then the following is sent:
    PC -> 192.168.0.100 UDP/NetBoot, "Request file '/KERNEL.IMG', off:1024, size:1024"
    If no send file request reached PC then another "Request file is sent":

    NOT SECURE
*/
#pragma once

#include <stdint.h>
#include <stdbool.h>



// Ontop of UDP
enum _NetBoot_Type {
    NETBOOT_REQUEST_FILE = 1,
    NETBOOT_SEND_FILE,
    NETBOOT_SEND_FILE_ACK,
    NETBOOT_REQUEST_FILE_LIST,
    NETBOOT_SEND_FILE_LIST,
};
typedef uint8_t NetBoot_Type;

#define NETBOOT_MAGIC "NBOO"
#define NETBOOT_DEFAULT_PORT 2493

// An ethernet frame can have about 1500 bytes without being fragmented.
// We use 1400 to make room for Ethernet, IPv4, UDP, and NetBoot headers.
#define NETBOOT_CHUNK_SIZE 1400


typedef struct NetBoot_Header {
    char         magic[4];
    uint16_t     version;
    NetBoot_Type type;
    uint8_t      _reserved;
} NetBoot_Header;

typedef struct NetBoot_Request_File_List {
    NetBoot_Header header;
    int            startIndex;
    int            fileCount;
} NetBoot_Request_File_List;

typedef struct NetBoot_Send_File_List {
    NetBoot_Header header;
    int            totalFiles;
    int            startIndex;
    int            fileCount;
    // struct {
    // uint8_t        filePath_len;
    // char           filePath;
    // } files[fileCount];
} NetBoot_Send_File_List;

typedef struct NetBoot_Request_File {
    NetBoot_Header header;
    uint64_t       offset;
    uint64_t       size;
    uint8_t        filePath_len; // does not include null termination
    char           filePath[]; // should be null terminated
} NetBoot_Request_File;

typedef struct NetBoot_Send_File {
    NetBoot_Header header;
    uint64_t       totalFileSize;
    uint64_t       offset;
    uint16_t       size;
    char           payload[];
} NetBoot_Send_File;

typedef struct NetBoot_Send_File_Ack {
    NetBoot_Header header;
    uint64_t       offset;
    uint16_t       size;
} NetBoot_Send_File_Ack;

typedef void* NetBoot_Device;

typedef int(*FN_NetBoot_recv)(NetBoot_Device device, void* buffer, int* out_size);
typedef int(*FN_NetBoot_send)(NetBoot_Device device, void* buffer, int size);

typedef struct NetBoot_Impl {
    FN_NetBoot_recv recv;
    FN_NetBoot_send send;
    uint8_t mac[6];
    NetBoot_Device device;
} NetBoot_Impl;

typedef struct NetBoot_Config {
    uint32_t  static_ip;
    uint32_t* server_ips;
    uint32_t  server_ips_len;
    uint16_t  port;
} NetBoot_Config;


//################################
//     CLIENT FUNCTIONS
//################################


bool NETBOOT_init(NetBoot_Impl* impl, NetBoot_Config* config);

// offset and size are optional
void NETBOOT_request_file_list(const char** files, int count);

// offset specifies offset into the file (not into the buffer, if you allocate memory for the whole file then YOU must pass buffer = fileMemory + offset)
// size specifies how many bytes to fetch OR the size of the buffer. If file is smaller than size.

int NETBOOT_request_file(const char* path, uint64_t offset, uint64_t size, void* buffer);

