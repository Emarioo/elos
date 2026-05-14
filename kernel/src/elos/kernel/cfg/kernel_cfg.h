#pragma once

#include "elos/common/types.h"

typedef struct KernelConfig {
    u32 static_ip;
    u32 netboot_ips[8];
    u32 netboot_ips_len;
    u16 netboot_port;

    u32 log_level;
} KernelConfig;


bool CFG_parse(const char* text, int text_len, KernelConfig* config, char** error);
