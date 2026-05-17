
#include "elos/execution.h"

#include "elos/kernel/exec/internal_exec.h"


#include "elos/common/types.h"
#include "elos/common/intrinsics.h"
#include "elos/common/string.h"
#include "elos/kernel_console.h"

#include "elos/physical_memory.h"

#include "elos/cpu.h"


#define printf(...) KCON_printf(__VA_ARGS__)


EXEC_Core cores[CORE_LIMIT];

bool scheduling_enabled;

void test_thread1();
void test_thread2();

void EXEC_terminate_self_end();
void thread_bootstrap(FN_ThreadEntry entry);

u64 EXEC_interrupt(InterruptFrame* frame) {
    int coreIndex = CPU_get_core_index();
    // printf("TIMED %d\n", coreIndex);

    if (!scheduling_enabled)
        return (u64)frame;

    u64 returnValue;
    EXEC_Core* core = &cores[coreIndex];


    LOCK(&core->thread_lock);

    int currentThread_index = core->active_thread;
    EXEC_Thread* currentThread = &core->threads[core->active_thread];
    EXEC_Thread* nextThread = NULL;

    if (frame->rip == (u64)EXEC_terminate_self_end) {
        currentThread->used = false;
    }

    while (1) {
        core->active_thread = (core->active_thread + 1) % THREAD_LIMIT;
        if (core->threads[core->active_thread].used || currentThread_index == core->active_thread) {
            nextThread = &core->threads[core->active_thread];
            break;
        }
    }

    if (currentThread == nextThread || nextThread == NULL) {
        // do nothing
        returnValue = (u64)frame;
        goto exit;
    }

    currentThread->frame = frame;
    returnValue = (u64)nextThread->frame;
    // printf("Switch to %d\n", core->active_thread);

exit:
    UNLOCK(&core->thread_lock);
    return returnValue;
}

void EXEC_init() {
    printf("EXEC init\n");

    int coreIndex = CPU_get_core_index();
    EXEC_Core* core = &cores[coreIndex];

    EXEC_Thread* this_thread = &core->threads[0];
    this_thread->used = true;
    core->active_thread = 0;

    // EXEC_create_thread(test_thread1);
    // EXEC_create_thread(test_thread2);

    printf("Enable scheduling\n");
    scheduling_enabled = true;
}

bool EXEC_create_thread(void* entry, int pinnedCoreIndex) {
    bool returnValue;
    int coreIndex;
    if (pinnedCoreIndex != -1) {
        if (pinnedCoreIndex < 0 || pinnedCoreIndex > CORE_LIMIT) {
            printf("EXEC_create_thread: Invalid pinnedCoreIndex %d (0 - %d)\n", pinnedCoreIndex, CORE_LIMIT-1);
            return false;
        }
        coreIndex = pinnedCoreIndex;
    } else {
        coreIndex = CPU_get_core_index();
    }
    EXEC_Core* core = &cores[coreIndex];

    LOCK_INT(&core->thread_lock);

    EXEC_Thread* found_thread = NULL;
    for (int i=0;i<ARRAY_LENGTH(core->threads);i++) {
        EXEC_Thread* thread = &core->threads[i];
        if (!thread->used) {
            found_thread = thread;
            break;
        }
    }
    if (!found_thread) {
        returnValue = false;
        goto exit;
    }

    // Terminated threads don't free their stack so we can reuse it.
    if (!found_thread->stack) {
        int stack_size = 0x10000;
        void* stack = PMEM_alloc(stack_size);
        if (!stack) {
            returnValue = false;
            goto exit;
        }
        found_thread->stack_size = stack_size;
        found_thread->stack = stack;
    }

    found_thread->used = true;
    found_thread->entry = entry;

    u64 rflags = get_rflags(); // usually IOPL and IF flags (IO permission and interrupt enable flag)
    u64 rsp = (u64)found_thread->stack + found_thread->stack_size;

    InterruptFrame* frame = (InterruptFrame*)(rsp - sizeof(InterruptFrame));
    found_thread->frame = frame;
    frame->cs = KERNEL_CODE_SEGMENT;
    frame->ss = KERNEL_DATA_SEGMENT;
    frame->rsp = rsp;
    frame->rflags = rflags;
    frame->rdi = (u64)entry;
    frame->rip = (u64)thread_bootstrap;

exit:
    UNLOCK_INT(&core->thread_lock);
    return returnValue;
}


void thread_bootstrap(FN_ThreadEntry entry) {
    entry();
    EXEC_terminate_self();
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




