#pragma once

#include "elos/common/types.h"

#include "elos/cpu.h"


typedef void(*FN_ThreadEntry)();

typedef struct {
    ContextFrame    frame;
    void*           stack;
    FN_ThreadEntry  entry;
    u32             stack_size;
    bool            used;
    bool            userSpace;
    bool            waitingForIO;
    u64             sleepUntilTick;

    const char*     elfBaseName;

    u16             pcid;

} EXEC_Thread;

#define THREAD_LIMIT 32
#define CORE_LIMIT 32

typedef struct {
    // @IMPORTANT DO NOT MOVE AROUND THESE FIELDS.
    //   They are hardcoded in assembly.
    void* syscall_stack;
    void* user_stack; // saved user stack on syscall
    bool  rescheduleSyscall;
    
    // Free to modify, not used by assembly
    u32   syscall_stack_size;
    EXEC_Thread threads[THREAD_LIMIT];
    EXEC_Thread idleThread; // kernel idle thread
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

bool EXEC_kill(const char* pattern);

void EXEC_terminate_self();

void EXEC_sleep(u64 ns);

void EXEC_timer_handler(ContextFrame* frame);

u64 EXEC_syscall_handler(u64 arg0, u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5);

void syscall_handler(); // defined in exec_support.s

void kernel_thread_reschedule(); // defined in exec_support.s

// @TODO Move elsewhere. Process resource manager perhaps.
void disableDefaultMonitorForUsers();
void enableDefaultMonitorForUsers();
