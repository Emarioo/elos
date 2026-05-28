#pragma once

#include "elos/common/types.h"

#include "elos/cpu.h"


typedef void(*FN_ThreadEntry)();

typedef struct {
    InterruptFrame* frame;
    void*           stack;
    FN_ThreadEntry  entry;
    u32             stack_size;
    bool            used;
    bool            userSpace;
} EXEC_Thread;

#define THREAD_LIMIT 32
#define CORE_LIMIT 32

typedef struct {
    EXEC_Thread threads[THREAD_LIMIT];
    int active_thread;
    volatile u32 thread_lock;
} EXEC_Core;

extern EXEC_Core cores[CORE_LIMIT];


void EXEC_init();

/*
    @param pinnedCoreIndex Specifies which core to pin the thread to. -1 for any core.
*/
bool EXEC_create_kernel_thread(void* entry, int pinnedCoreIndex);

/*
    @param pinnedCoreIndex Specifies which core to pin the thread to. -1 for any core.
*/
bool EXEC_create_user_thread(const char* path, int pinnedCoreIndex);

void EXEC_terminate_self();

u64 EXEC_interrupt(InterruptFrame* frame);
