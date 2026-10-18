#pragma once

#include "elos/common/types.h"

#include "elos/cpu.h"
#include "elos/elos.h"

#include "elos/permission.h"


typedef void(*FN_ThreadEntry)();

typedef struct EXEC_Process EXEC_Process;
typedef struct EXEC_Thread EXEC_Thread;

struct EXEC_Process {
    Permissions* permissions;

    const char*     elfBaseName;
    ELOS_ProcessID  processID;
    volatile bool   killed;

    void*        pageTable;

    u32          threads_len;
    u32          threads_cap;
    EXEC_Thread* threads;
};

typedef enum {
    EXEC_WAITFOR_ASYNC = 0x1,
    EXEC_WAITFOR_PERM_REQUEST = 0x2,
} EXEC_WaitForMask;

struct EXEC_Thread {
    ContextFrame    frame;
    void*           stack;
    void*           phys_stack;
    FN_ThreadEntry  entry;
    u32             stack_size;
    bool            userSpace;
    volatile bool   killed;
    volatile u32    waitingForMask;
    volatile bool   waitingForIO;
    bool            compatMode;
    u64             sleepUntilTick;

    u16             pcid;

    EXEC_Process*   process;

};

#define CORE_LIMIT 32
#define IDLE_THREAD_INDEX 0
#define INITIAL_THREAD_INDEX 1

typedef struct {
    // @IMPORTANT DO NOT MOVE AROUND THESE FIELDS.
    //   They are hardcoded in assembly.
    void* syscall_stack;
    void* user_stack; // saved user stack on syscall
    bool  rescheduleSyscall;
    
    // Free to modify, not used by assembly
    u32           syscall_stack_size;
    u32           threads_cap;
    u32           threads_len;
    EXEC_Thread** threads;
    int           active_thread;
    volatile u32  thread_lock;
    EXEC_Thread   idle_thread;
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
ELOS_Error EXEC_create_user_process(const char* path);

bool EXEC_kill(const char* pattern);
void EXEC_kill_process(EXEC_Process* process);

// Only called by kernel threads
void EXEC_terminate_self();

void EXEC_sleep(u64 ns);
void EXEC_wake_up_threads(EXEC_Process* process);

EXEC_Thread* EXEC_get_active_thread();

void EXEC_timer_handler(ContextFrame* frame);


u64 EXEC_syscall_handler(u64 arg0, u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5);

void syscall_handler(); // defined in exec_support.s
void syscall_handler32(); // defined in exec_support.s

void kernel_thread_reschedule(); // defined in exec_support.s

// @TODO Move elsewhere. Process resource manager perhaps.
void disableDefaultMonitorForUsers();
void enableDefaultMonitorForUsers();
