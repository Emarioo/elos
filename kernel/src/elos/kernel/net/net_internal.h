#pragma once

#include "elos/common/types.h"

#include "elos/network.h"
#include "elos/kernel/driver/pci.h"

#include "elos/kernel/driver/pci_list.h"


#define MAX_NET_DEVICES 10

extern NET_Device g_devices[MAX_NET_DEVICES];
extern int        g_devices_len;
extern int        g_devices_max;

// extern FN_NET_recv_packet g_recv_packet_callback;
// extern void* g_recv_packet_callback_userData;


bool net_handle_packet(NET_Device* device, NET_Packet* packet);

bool fetch_mac_from_address(u32 address, NET_Device** out_device, u8 out_mac[6]);

void arp_update_table(u32 address, NET_Device* device, u8 mac[6]);
