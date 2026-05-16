#pragma once

#include "elos/boot_api.h"

#include "elos/common/types.h"


#define KERNEL_CODE_SEGMENT 0x8
#define KERNEL_DATA_SEGMENT 0x10


void CPU_init(BootAPI* boot_api);


void CPU_enable_interrupt();
void CPU_disable_interrupt();

void CPU_reset();

void CPU_sleep(u64 nanoseconds);

int CPU_get_core_index();

