#pragma once

#include "elos/common/types.h"

#include "elos/network.h"

extern u32 current_ip;
extern u8 current_mac[6];

extern FN_NET_recv_packet g_recv_packet_callback;
extern void* g_recv_packet_callback_userData;
