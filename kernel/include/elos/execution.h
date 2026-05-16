#pragma once

#include "elos/common/types.h"

#include "elos/cpu.h"



void EXEC_init();

bool EXEC_create_thread(void* entry);

u64 EXEC_interrupt(InterruptFrame* frame);
