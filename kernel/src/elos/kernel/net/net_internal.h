#pragma once

#include "elos/common/types.h"

extern u32 current_ip;
extern u8 current_mac[6];

void handle_packet(void* buffer, int length);
