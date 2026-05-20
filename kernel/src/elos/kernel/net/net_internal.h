#pragma once

#include "elos/common/types.h"

#include "elos/network.h"
#include "elos/kernel/driver/pci.h"

#include "elos/kernel/driver/pci_list.h"

extern u32 current_ip;
extern u8 current_mac[6];

extern FN_NET_recv_packet g_recv_packet_callback;
extern void* g_recv_packet_callback_userData;


typedef struct NetworkController {
    bool found;
    PCI_ConfigSpace config;
    u64 ioaddr;
    u64 ioaddr_size;
    u64 maddr;
    u64 maddr_size;

    u8 mac_address[6];
} NetworkController;

extern NetworkController controller;

