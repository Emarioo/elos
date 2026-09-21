/*

    Small but useful driver for RTL8169 Network Controller

    Supports:
        - Init, shutdown
        - Poll and send packets (not blocking)
        - Interrupt with callback to receive packets

    Requires:
        - PCI enumeration (to find controller on PCI bus)
        - Memory allocator (for packet buffers)

*/

#pragma once 

#include "elos/common/types.h"
#include "elos/network.h"

// @TODO RTL8169 registers


bool rtl8169_init(NET_Device* device);
u32 rtl8169_send_packet(const void* data, u32 size);
void rtl8169_receive_packet(void** out_buffer, u32* out_size);
