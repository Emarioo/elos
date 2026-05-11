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



bool rtl8169_init();

// @TODO Shutdown

void rtl8169_receive_packet(void** out_buffer, int* out_size);

int rtl8169_send_packet(void* data, int size);

// @TODO Interrupt and callback
