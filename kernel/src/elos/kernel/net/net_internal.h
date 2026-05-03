#pragma once

#include "elos/common/types.h"

#include "elos/network.h"
#include "elos/kernel/driver/pci.h"

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


// Intel Corporation
#define VENDOR_ID__INTEL     0x8086
// 82540EM Gigabit Ethernet Controller
#define DEVICE_ID__82540EM_A 0x100E

// Realtek Semiconductor Co., Ltd.
#define VENDOR_ID__REALTEK   0x10ec
// RTL8111/8168/8411 PCI Express Gigabit Ethernet Controller
#define DEVICE_ID__RTL8169   0x8168
// RTL8822CE 802.11ac PCIe Wireless Network Adapter
#define DEVICE_ID__RTL8822CE 0xc822
