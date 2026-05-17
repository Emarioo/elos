#pragma once

#include "elos/common/types.h"

#include "elos/cpu.h"



void EXEC_init();

/*
    @param pinnedCoreIndex Specifies which core to pin the thread to. -1 for any core.
*/
bool EXEC_create_thread(void* entry, int pinnedCoreIndex);

void EXEC_terminate_self();

u64 EXEC_interrupt(InterruptFrame* frame);
