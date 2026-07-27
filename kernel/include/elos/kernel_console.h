#pragma once

#include "elos/boot_api.h"

#include "elos/network.h"

typedef void(*FN_KCON_write)(const char*, int);

void KCON_init(BootAPI* boot_api);
void KCON_add_write_hook(FN_KCON_write func);

void KCON_printf(const char* format, ...);


void KCON_net_set_target(NetDevice device, u8 mac[6], u32 address);

void KCON_net_write(const char* buffer, int len);

void kernel_panic(const char* format, ...);

#define KERNEL_PANIC(expression) ((expression) ? true : (kernel_panic("[PANIC] %s (%s:%u)\n",#expression,__FILE__,__LINE__)))
