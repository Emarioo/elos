
#include "elos/execution.h"

#include "elos/kernel/exec/internal_exec.h"


#include "elos/common/types.h"
#include "elos/common/intrinsics.h"
#include "elos/common/string.h"
#include "elos/kernel_console.h"

#include "elos/kernel/exec/read_elf.h"

#include "elos/physical_memory.h"

#include "elos/cpu.h"
#include "elos/keyboard.h"



#define printf(...) KCON_printf(__VA_ARGS__)


EXEC_Core cores[CORE_LIMIT];

bool scheduling_enabled;

void thread_idle_loop();
void thread_bootstrap(FN_ThreadEntry entry);

void EXEC_timer_handler(ContextFrame* frame) {
    int coreIndex = CPU_get_core_index();

    if (coreIndex == 0) {
        // @TODO Our intention is to do this on one core.
        //   The platform does not guarrante that core 0 is available.
        //   It may be defective where core 1 is the boot core instead.
        KBD_tick_handler();
    }

    if (!scheduling_enabled)
        return;

    EXEC_Core* core = &cores[coreIndex];


    LOCK(&core->thread_lock);


    int currentThread_index = core->active_thread;
    EXEC_Thread* currentThread = &core->threads[currentThread_index];
    EXEC_Thread* nextThread = NULL;

    // printf("Not going %d %d\n", currentThread_index, core->active_thread);

    u64 nowTick = rdtsc();

    int limit = THREAD_LIMIT;
    while (1) {
        limit--;
        if (limit < 0) {
            core->active_thread = IDLE_THREAD_INDEX;
            nextThread = &core->threads[IDLE_THREAD_INDEX];
            break;
        }

        core->active_thread = (core->active_thread + 1) % THREAD_LIMIT;
        EXEC_Thread* thread = &core->threads[core->active_thread];
        // if (thread->used) {
        //     printf("Try reschedule %d io=%d now=%zu sleep=%zu\n", core->active_thread, thread->waitingForIO, (nowTick/1000), (thread->sleepUntilTick/1000));
        // }
        if (thread->used && !thread->waitingForIO && nowTick > thread->sleepUntilTick) {
            nextThread = thread;
            thread->sleepUntilTick = 0;
            break;
        }
    }
    // printf("Timer int cur=%d act=%d next=%p\n", currentThread_index, core->active_thread, nextThread);
    if (currentThread == nextThread) {
        // printf("Single thread\n");
        // There's only one thread.
        frame->ss |= frame->cs & 3;
    } else {
        memcpy(&currentThread->frame, frame, sizeof(*frame));
        memcpy(frame, &nextThread->frame, sizeof(*frame));

        if (!currentThread->userSpace) {
            // If we are on a kernel thread (not the original one at boot) then when popping the frame
            // we will switch page table and tripple fault because the stack isn't mapped.
            // We will implement high half kernel which will solve this problem but for now we make sure
            // it's mapped here (very inefficient).
            PMEM_map_memory((PageTable*)frame->cr3, currentThread->stack, currentThread->stack, currentThread->stack_size, PMEM_FLAG_NONE);
        }

        // SS privilege gets cleared on my laptop (not in QEMU).
        // May be doing something wrong but
        // this ensures we get right privilege.
        frame->ss |= frame->cs & 3;

        // printf("Switch\n");
        // printf(" ss=%x, from %x\n", frame->ss, currentThread->frame.ss);
        // printf(" cs=%x, from %x\n", frame->cs, currentThread->frame.cs);
    }

    if (nextThread->compatMode) {
        // Change return address. So we use iret with 32-bit operand size instead.
        *(u64*)((char*)frame - 8) = (u64)timer_isr_ret32;

        frame->compat.eip    = frame->rip;
        frame->compat.cs     = frame->cs;
        frame->compat.eflags = frame->rflags;
        frame->compat.esp    = frame->rsp;
        frame->compat.ss     = frame->ss;
    }

exit:
    UNLOCK(&core->thread_lock);
    return;
}

void EXEC_init() {
    int coreIndex = CPU_get_core_index();
    EXEC_Core* core = &cores[coreIndex];

    {
        core->active_thread = 0;
        EXEC_Thread* this_thread = &core->threads[core->active_thread];
        this_thread->used = true;
    }

    {
        EXEC_Thread* idle_thread = &core->threads[IDLE_THREAD_INDEX];
        idle_thread->entry = thread_idle_loop;
        idle_thread->used = false; // used is irrelevant for idle_thread, it always exists.
        
        int stack_size = 0x2000;
        void* stack = PMEM_alloc(stack_size);
        KERNEL_PANIC(stack, "Cannot make stack for idle thread\n");
        
        idle_thread->stack_size = stack_size;
        idle_thread->stack = stack;

        u64 rsp = (u64)idle_thread->stack + idle_thread->stack_size;
        rsp -= 0x8; // So that we at _start when we push rbp have 16-byte aligned stack.

        ContextFrame* frame = &idle_thread->frame;
        memset(frame, 0, sizeof(*frame));
        frame->cs = KERNEL_CODE_SEGMENT;
        frame->ss = KERNEL_DATA_SEGMENT;
        frame->rsp = rsp;
        frame->rflags = 0x200; // interrupt flag
        frame->rip = (u64)idle_thread->entry;
        frame->cr3 = (u64)g_kernelPageTable;
    }
    
    // @TODO Is this the first core?
    if (coreIndex == 0) {
        printf("Enable scheduling\n");
        scheduling_enabled = true;
    }
}

EXEC_Thread* EXEC_get_active_thread() {
    int coreIndex = CPU_get_core_index();
    EXEC_Core* core = &cores[coreIndex];
    return &core->threads[core->active_thread];
}

bool EXEC_create_kernel_thread(void* entry, int pinnedCoreIndex) {
    bool returnValue = false;
    int coreIndex;
    if (pinnedCoreIndex != -1) {
        if (pinnedCoreIndex < 0 || pinnedCoreIndex > CORE_LIMIT) {
            printf("EXEC_create_kernel_thread: Invalid pinnedCoreIndex %d (0 - %d)\n", pinnedCoreIndex, CORE_LIMIT-1);
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
        goto exit;
    }

    // Terminated threads don't free their stack so we can reuse it.
    if (!found_thread->stack) {
        int stack_size = 0x10000;
        void* stack = PMEM_alloc(stack_size);
        if (!stack) {
            goto exit;
        }
        found_thread->stack_size = stack_size;
        found_thread->stack = stack;
    }

    found_thread->used = true;
    found_thread->entry = entry;

    u64 rsp = (u64)found_thread->stack + found_thread->stack_size;
    rsp -= 0x8; // So that we at _start when we push rbp have 16-byte aligned stack.

    ContextFrame* frame = &found_thread->frame;
    memset(frame, 0, sizeof(*frame));
    frame->cs = KERNEL_CODE_SEGMENT;
    frame->ss = KERNEL_DATA_SEGMENT;
    frame->rsp = rsp;
    frame->rflags = 0x200; // interrupt flag
    frame->rdi = (u64)entry;
    frame->rip = (u64)thread_bootstrap;
    frame->cr3 = (u64)g_kernelPageTable;

exit:
    UNLOCK_INT(&core->thread_lock);
    return returnValue;
}

bool EXEC_create_user_thread(const char* path, int pinnedCoreIndex) {
    bool returnValue = false;

    ElfObject object;
    bool result = read_elf(path, &object);

    if (!result) {
        return false;
    }

    int coreIndex;
    if (pinnedCoreIndex != -1) {
        if (pinnedCoreIndex < 0 || pinnedCoreIndex > CORE_LIMIT) {
            printf("EXEC_create_kernel_thread: Invalid pinnedCoreIndex %d (0 - %d)\n", pinnedCoreIndex, CORE_LIMIT-1);
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
        goto exit;
    }

    // @TODO We should free stack from terminated threads.
    //   Reusing for user mode is bad.
    // if (!found_thread->stack) { }

    int stack_size = 0x10000; // @TODO Increase
    void* phys_stack = PMEM_alloc_phys(stack_size, PMEM_FLAG_NONE);
    if (!phys_stack) {
        goto exit;
    }
    void* virt_stack = (void*)(u64)0xF0000000;
    PMEM_map_memory(g_kernelPageTable, virt_stack, phys_stack, stack_size, PMEM_FLAG_USER_SPACE);
    memset(virt_stack, 0x9A, stack_size);
    PMEM_map_memory(object.pageTable, virt_stack, phys_stack, stack_size, PMEM_FLAG_USER_SPACE);

    found_thread->stack_size = stack_size;
    found_thread->stack = virt_stack;

    found_thread->used = true;
    found_thread->userSpace = true;
    found_thread->entry = object.entry_point;
    found_thread->compatMode = object.compatibilityMode;

    u64 rsp = (u64)found_thread->stack + found_thread->stack_size;
    rsp -= 0x8; // So that we at _start when we push rbp have 16-byte aligned stack.

    ContextFrame* frame = &found_thread->frame;
    memset(frame, 0, sizeof(*frame));
    if (object.compatibilityMode) {
        frame->cs = USER_CODE_COMPATIBILITY_SEGMENT | 3;
        frame->ss = USER_DATA_COMPATIBILITY_SEGMENT | 3;
    } else {
        frame->cs = USER_CODE_SEGMENT | 3;
        frame->ss = USER_DATA_SEGMENT | 3;
    }
    frame->rsp = rsp;
    frame->rflags = 0x202; // interrupt flag, disable IOPL
    frame->rip = (u64)object.entry_point;
    frame->cr3 = (u64)object.pageTable;

    // This is so dumb. a page for a tiny little string
    char* name = PMEM_alloc_phys(4096, PMEM_FLAG_IDENTITY_MAPPED);
    char* slashPos = strrchr(path, '/');
    if (slashPos) {
        strncpy(name, slashPos + 1, 4096);
    } else {
        strncpy(name, path, 4096);
    }
    found_thread->elfBaseName = name;

exit:
    // @TODO Cleanup allocated stuff.
    UNLOCK_INT(&core->thread_lock);
    return returnValue;
}

void thread_bootstrap(FN_ThreadEntry entry) {
    entry();
    EXEC_terminate_self();
}

void thread_idle_loop() {
    while (1) {
        int coreIndex = CPU_get_core_index();
        // printf("Idle core %d\n", coreIndex);
        // Halts execution until an interrupt.
        // On timer interrupt we will check if a
        // thread is done sleeping and switch to it,
        // otherwise keep idling.
        asm volatile ( "hlt" );
    }
}


void EXEC_terminate_self() {
    CPU_disable_interrupt();

    u32 coreIndex = CPU_get_core_index();
    EXEC_Core* core = &cores[coreIndex];
    EXEC_Thread* activeThread = &core->threads[core->active_thread];
    activeThread->used = false;

    CPU_enable_interrupt();

    // printf("Terminating\n");

    while (1) {
        asm volatile ( "hlt" );
    }
}


void EXEC_sleep(u64 sleepTime_ns) {
    CPU_disable_interrupt();

    int coreIndex = CPU_get_core_index();
    EXEC_Core* core = &cores[coreIndex];
    EXEC_Thread* activeThread = &core->threads[core->active_thread];

    u64 nowTick = rdtsc();
    u64 sleepTick = nowTick + (sleepTime_ns * CPU_ticks_per_second()/100) / 10000000;
    activeThread->sleepUntilTick = sleepTick;

    // printf("Sleep at %zu\n", nowTick);
    kernel_thread_reschedule();
    u64 endTick = rdtsc();

    // printf("Sleep at %zu, %zu\n", endTick, endTick - nowTick);

    u64 ms = (endTick - nowTick) / (CPU_ticks_per_second()/1000);
    // In QEMU on my laptop with power cable in if we sleep for 16.66 ms we seem to
    // sleep for about 18-20ms.
    // @TODO Investigate on real hardware how we can make sleep more precise.
    //   We need more user applications to put load on the system when testing.
    // printf("Sleeped for %d ms (%d us)\n", ms, sleepTime_ns/1000);

    CPU_enable_interrupt();
}


bool EXEC_kill(const char* pattern) {
    bool foundAny = false;
    for (int i=0;i<CORE_LIMIT;i++) {
        EXEC_Core* core = &cores[i];
        LOCK_INT(&core->thread_lock);
    }

    for (int ci=0;ci<CORE_LIMIT;ci++) {
        EXEC_Core* core = &cores[ci];

        for (int ti=0;ti<THREAD_LIMIT;ti++) {
            EXEC_Thread* thread = &core->threads[ti];
            if (!thread->used || !thread->userSpace)
                continue;

            char* res = strstr(thread->elfBaseName, pattern);
            if (res) {
                // @TODO Free resources.
                //    Maybe notify thread it's being killed and give it 500ms until it's forcefully terminated?
                printf("Killed %s, core=%d thread=%d\n", thread->elfBaseName, ci, ti);
                thread->used = false;
                foundAny = true;
            }
        }
    }

    for (int i=CORE_LIMIT-1;i>=0;i--) {
        EXEC_Core* core = &cores[i];
        UNLOCK_INT(&core->thread_lock);
    }

    return foundAny;
}
