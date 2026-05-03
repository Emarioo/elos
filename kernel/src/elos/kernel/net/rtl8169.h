#pragma once 

#include "elos/common/types.h"


typedef struct Descriptor {
    u32 command;
    u32 vlan;
    u32 low_buf;
    u32 high_buf;
} Descriptor;

#define DESCRIPTOR_COMMAND_OWN (1 << 31)
#define DESCRIPTOR_COMMAND_EOR (1 << 30)
#define DESCRIPTOR_COMMAND_FS (1 << 29)
#define DESCRIPTOR_COMMAND_LS (1 << 28)

bool rtl8169_init();

void rtl8169_receive_packet(void** out_buffer, int* out_size);

int rtl8169_send_packet(void* data, int size);

