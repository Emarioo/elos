#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "netboot/netboot.h"
#include "elos/kernel/cfg/kernel_cfg.h"

#include <efi.h>
#include <efilib.h>

extern bool can_load_kernel_from_network;
extern EFI_SIMPLE_NETWORK_PROTOCOL* simple_network;


void init_network();

void KCON_printf(const char* format, ...);

#define printf(...) KCON_printf(__VA_ARGS__)

extern NetBoot_Impl netboot_impl;
extern NetBoot_Config netboot_config;
extern NetBoot_Device netboot_device;

extern KernelConfig kernel_config;
