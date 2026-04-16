#pragma once

#include "elos/boot_api.h"

typedef void(*FN_KCON_write)(const char*, int);

void KCON_init(BootAPI* boot_api);
void KCON_add_printf_hook(FN_KCON_write func);

void KCON_printf(const char* format, ...);


