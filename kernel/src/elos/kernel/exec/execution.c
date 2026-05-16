
#include "elos/execution.h"

#include "elos/kernel/exec/internal_exec.h"


#include "elos/common/types.h"
#include "elos/common/intrinsics.h"
#include "elos/common/string.h"
#include "elos/kernel_console.h"

#include "elos/physical_memory.h"

#include "elos/cpu.h"


#define printf(...) KCON_printf(__VA_ARGS__)


typedef void(*FN_ThreadEntry)();

typedef struct {
    bool used;
    InterruptFrame* frame;
    FN_ThreadEntry entry;
} EXEC_Thread;

#define THREAD_LIMIT 32
#define CORE_LIMIT 32

typedef struct {
    EXEC_Thread threads[THREAD_LIMIT];
    int active_thread;
} EXEC_Core;

EXEC_Core cores[CORE_LIMIT];

bool scheduling_enabled;

void test_thread1();
void test_thread2();


u64 EXEC_interrupt(InterruptFrame* frame) {
    if (!scheduling_enabled)
        return (u64)frame;

    int coreIndex = CPU_get_core_index();
    EXEC_Core* core = &cores[coreIndex];

    int currentThread_index = core->active_thread;
    EXEC_Thread* currentThread = &core->threads[core->active_thread];
    EXEC_Thread* nextThread = NULL;

    while (1) {
        core->active_thread = (core->active_thread + 1) % THREAD_LIMIT;
        if (core->threads[core->active_thread].used || currentThread_index == core->active_thread) {
            nextThread = &core->threads[core->active_thread];
            break;
        }
    }

    if (currentThread == nextThread) {
        // do nothing
        return (u64)frame;
    }

    // printf("Switch to %d\n", core->active_thread);

    currentThread->frame = frame;
    return (u64)nextThread->frame;
}

void EXEC_init() {
    printf("EXEC init\n");


    EXEC_create_thread(test_thread1);
    EXEC_create_thread(test_thread2);

    EXEC_Core* core = &cores[0];

    core->active_thread = THREAD_LIMIT-1;
    if (core->threads[core->active_thread].used) {
        printf("Overwriting FRAME of created thread #%d!!!\n", core->active_thread);
    }

    printf("Enable scheduling\n");
    scheduling_enabled = true;
    // Wait for timer interrupt
    while (1) {
        pause();
    }
}

bool EXEC_create_thread(void* entry) {
    EXEC_Core* core = &cores[0];
    
    // @TODO Thread safety
    EXEC_Thread* found_thread = NULL;
    for (int i=0;i<ARRAY_LENGTH(core->threads);i++) {
        EXEC_Thread* thread = &core->threads[i];
        if (!thread->used) {
            found_thread = thread;
            break;
        }
    }
    if (!found_thread) {
        return false;
    }
    memset(found_thread, 0, sizeof(*found_thread));

    int stack_size = 0x10000;
    void* stack = PMEM_alloc(stack_size);
    if (!stack)
        return false;

    found_thread->used = true;
    found_thread->entry = entry;

    u64 rflags = get_rflags(); // usually IOPL and IF flags (IO permission and interrupt enable flag)
    u64 rsp = (u64)stack + stack_size;

    InterruptFrame* frame = (InterruptFrame*)(rsp - sizeof(InterruptFrame));
    found_thread->frame = frame;
    frame->cs = KERNEL_CODE_SEGMENT;
    frame->ss = KERNEL_DATA_SEGMENT;
    frame->rsp = rsp;
    frame->rflags = rflags;
    frame->rip = (u64)entry;

    return true;
}

void thread_bootstrap(FN_ThreadEntry entry) {
    entry();

    // @TODO Terminate thread
    while (1) asm ( "hlt" );
}


void test_thread1() {
    while(1) {
        printf("Cat\n");
        CPU_sleep(100000000);
    }
}

void test_thread2() {
    while(1) {
        printf("Dog\n");
        CPU_sleep(100000000);
    }
}




