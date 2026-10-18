
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

EXEC_Process* g_processes;
u32           g_processes_len;
u32           g_processes_cap;
volatile u32  g_nextProcessID;

EXEC_Thread*  g_kernel_threads;
u32           g_kernel_threads_len;
u32           g_kernel_threads_cap;

volatile u32 g_process_lock;

EXEC_Process* create_process() {
    EXEC_Process* returnValue;
    LOCK_INT(&g_process_lock);
    
    if (!g_processes) {
        // @TODO x86_64 has a max page context identifier of 4096.
        //    That is the number of processes with efficient page mapping we can have.
        //    That might be a good process limit.
        g_processes_cap = 1000;
        g_processes = PMEM_alloc(sizeof(EXEC_Process) * g_processes_cap);
        KERNEL_PANIC(g_processes, "Could not allocate process array");
        g_processes_len = 0;
    }

    EXEC_Process* process = &g_processes[g_processes_len];
    g_processes_len++;
    memset(process, 0, sizeof(*process));

    returnValue = process;
    
    UNLOCK_INT(&g_process_lock);
    return returnValue;
}

EXEC_Thread* create_thread(EXEC_Process* process) {
    EXEC_Thread* returnValue;
    LOCK_INT(&g_process_lock);
    
    if (!process->threads) {
        process->threads_cap = 4;
        process->threads = PMEM_alloc(sizeof(EXEC_Thread) * process->threads_cap);
        KERNEL_PANIC(process->threads, "Could not allocate thread array for process");
        memset(process->threads, 0, sizeof(EXEC_Thread) * process->threads_cap);
        process->threads_len = 0;
    }

    EXEC_Thread* thread = &process->threads[process->threads_len];
    process->threads_len++;
    memset(thread, 0, sizeof(*thread));

    returnValue = thread;
    
    UNLOCK_INT(&g_process_lock);
    return returnValue;
}

EXEC_Thread* create_kernel_thread() {
    EXEC_Thread* returnValue;
    LOCK_INT(&g_process_lock);
    
    if (!g_kernel_threads) {
        g_kernel_threads_cap = 64;
        g_kernel_threads = PMEM_alloc(sizeof(EXEC_Thread) * g_kernel_threads_cap);
        KERNEL_PANIC(g_kernel_threads, "could not allocate kernel thread array");
        g_kernel_threads_len = 0;
    }

    EXEC_Thread* thread = &g_kernel_threads[g_kernel_threads_len];
    g_kernel_threads_len++;
    memset(thread, 0, sizeof(*thread));

    returnValue = thread;
    
    UNLOCK_INT(&g_process_lock);

    return returnValue;
}

bool EXEC_is_boot_core() {
    int coreIndex = CPU_get_core_index();
    // @TODO We currently don't guarrantee 0 is boot core.
    return coreIndex == 0;
}
// assumes core/thread is locked already
int reserve_free_core_thread_index() {
    int coreIndex = CPU_get_core_index();
    EXEC_Core* core = &cores[coreIndex];
    
    if (core->threads_len >= core->threads_cap) {
        return -1;
    }
    int thread_index = core->threads_len;
    core->threads_len++;
    return thread_index;
}

// Implementation-wise the action is queued.
// The execution is stopped but kernel may be using some of the memory buffers
// so we wait until all resources are done being busy.
void kill_process_queued(EXEC_Process* process) {
    // @TODO Implement kill process
}


bool scheduling_enabled;

void thread_idle_loop();
void thread_bootstrap(FN_ThreadEntry entry);

void EXEC_timer_handler(ContextFrame* frame) {

    if (EXEC_is_boot_core()) {
        KBD_tick_handler();
    }

    if (!scheduling_enabled)
        return;

    int coreIndex = CPU_get_core_index();
    EXEC_Core* core = &cores[coreIndex];

    // printf("Interrupt %d\n", coreIndex);

    LOCK(&core->thread_lock);


    int currentThread_index = core->active_thread;
    EXEC_Thread* currentThread = core->threads[currentThread_index];
    EXEC_Thread* nextThread = NULL;

    // printf("Not going %d %d\n", currentThread_index, core->active_thread);

    u64 nowTick = rdtsc();

    int limit = core->threads_len;
    while (1) {
        limit--;
        if (limit < 0) {
            core->active_thread = IDLE_THREAD_INDEX;
            nextThread = core->threads[IDLE_THREAD_INDEX];
            break;
        }

        core->active_thread = 1 + (core->active_thread) % (core->threads_len-1);
        EXEC_Thread* thread = core->threads[core->active_thread];
        // if (thread->used) {
        //     printf("Try reschedule %d io=%d now=%zu sleep=%zu\n", core->active_thread, thread->waitingForIO, (nowTick/1000), (thread->sleepUntilTick/1000));
        // }

        if (!thread) {
            continue;
        }

        if (thread->killed) {
            // kill thread and remove from list
            core->threads_len--;
            core->threads[core->active_thread] = core->threads[core->threads_len];
            core->active_thread--;

            EXEC_Process* process = thread->process;
            // @TODO Free thread resources.
            // @TODO Free process resources if zero threads remain.
            //   If another core is using some of the processe's allocated memory like async handler then
            //   delay resource cleanup for process.

            continue;
        }

        if (!thread->waitingForIO && 0 == thread->waitingForMask && nowTick > thread->sleepUntilTick) {
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

        // SS privilege gets cleared on my laptop (not in QEMU).
        // May be doing something wrong but
        // this ensures we get right privilege.
        frame->ss |= frame->cs & 3;

        // printf("Switch\n");
        // printf(" ss=%x, from %x\n", frame->ss, currentThread->frame.ss);
        // printf(" cs=%x, from %x\n", frame->cs, currentThread->frame.cs);
    }

    if (nextThread->compatMode) {
        // Change return address of this stack frame. So we use iret with 32-bit operand size instead.
        *(u64*)((char*)frame - 8) = (u64)timer_isr_ret32;

        frame->compat.eip    = frame->rip;
        frame->compat.cs     = frame->cs;
        frame->compat.eflags = frame->rflags;
        frame->compat.esp    = frame->rsp;
        frame->compat.ss     = frame->ss;
    }
    
    // if(nextThread->userSpace) {
    //     PageTable* oldt = PMEM_phys_to_kernel((PageTable*)currentThread->frame.cr3);
    //     PageTable* newt = PMEM_phys_to_kernel((PageTable*)frame->cr3);

    //     printf("cr3 %zx\n", frame->cr3);
    // }

exit:
    UNLOCK(&core->thread_lock);
    return;
}


void EXEC_init() {
    int coreIndex = CPU_get_core_index();
    EXEC_Core* core = &cores[coreIndex];

    core->threads_cap = PAGE_SIZE/sizeof(void*);
    core->threads_len = 1;
    core->threads = PMEM_alloc(core->threads_cap * sizeof(void*));
    memset(core->threads, 0, core->threads_cap * sizeof(void*));

    {
        EXEC_Thread* idle_thread = &core->idle_thread;
        core->threads[IDLE_THREAD_INDEX] = idle_thread;
        idle_thread->entry = thread_idle_loop;
        
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
        frame->cr3 = (u64)PMEM_kernel_to_phys(g_kernelPageTable);
    }
    
    EXEC_Thread* this_thread = create_kernel_thread();
    core->active_thread = reserve_free_core_thread_index();
    core->threads[core->active_thread] = this_thread;
    
    
    // @TODO Is this the first core?
    if (EXEC_is_boot_core()) {
        // printf("Enable scheduling\n");
        scheduling_enabled = true;
    }
}

EXEC_Thread* EXEC_get_active_thread() {
    int coreIndex = CPU_get_core_index();
    EXEC_Core* core = &cores[coreIndex];
    return core->threads[core->active_thread];
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

    int free_index = reserve_free_core_thread_index();
    if (free_index == -1) {
        printf("EXEC_create_kernel_thread: Could not get free core thread index %d/%d\n", core->threads_len, core->threads_cap);
        goto exit;
    }

    EXEC_Thread* found_thread = create_kernel_thread();
    KERNEL_PANIC(found_thread, "could not create kernel thread");
    core->threads[free_index] = found_thread;

    int stack_size = 0x10000;
    void* stack = PMEM_alloc(stack_size);
    KERNEL_PANIC(stack, "could not allocate stack");
    
    found_thread->stack_size = stack_size;
    found_thread->stack = stack;
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
    frame->cr3 = (u64)PMEM_kernel_to_phys(g_kernelPageTable);

    returnValue = true;

exit:
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
    EXEC_Thread* activeThread = core->threads[core->active_thread];
    KERNEL_PANIC(!activeThread->userSpace, "terminate self can only be called by kernel threads");
    activeThread->killed = true;

    CPU_enable_interrupt();

    // @TODO Trigger reschedule.

    // printf("Terminating\n");

    while (1) {
        asm volatile ( "hlt" );
    }
}


void EXEC_sleep(u64 sleepTime_ns) {
    CPU_disable_interrupt();

    int coreIndex = CPU_get_core_index();
    EXEC_Core* core = &cores[coreIndex];
    EXEC_Thread* activeThread = core->threads[core->active_thread];

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

void EXEC_wake_up_threads(EXEC_Process* process) {
    // @TODO Implement different waitingForIO.
    //   One for ASYNC.
    //   One for other signals/events like process finished, thread finished, interrupt/event from USB device.
    //   Potentially all this goes through ASYNC API where you submit wait_for_event requests.
    
    LOCK_INT(&g_process_lock);

    for (int i=0;i<process->threads_len;i++) {
        process->threads[i].waitingForIO = false;
    }
    
    UNLOCK_INT(&g_process_lock);
}

bool pattern_match(const char* haystack, const char* needle) {
    int lena = strlen(haystack);
    int lenb = strlen(needle);

    int correct = 0;
    int index = 0;
    while (index + lenb <= lena) {
        char chr = haystack[index];
        char echr = needle[correct];
        // case-insensitive matching
        if (chr == echr || ((chr|32) == (echr|32) && (chr|32) >= 'a' && (chr|32) <= 'z' && (echr|32) >= 'a' && (echr|32) <= 'z')) {
            correct++;
            if (correct == lenb) {
                return true;
            }
        }
        index++;
    }
    return false;
}

bool EXEC_kill(const char* pattern) {
    bool foundAny = false;
    for (int i=0;i<CORE_LIMIT;i++) {
        EXEC_Core* core = &cores[i];
        LOCK_INT(&core->thread_lock);
    }
    
    LOCK_INT(&g_process_lock);

    for (int pi=0;pi<g_processes_len;pi++) {
        EXEC_Process* process = &g_processes[pi];
        if (process->killed) {
            continue;
        }

        bool res = pattern_match(process->elfBaseName, pattern);
        if (res) {
            foundAny = true;
            // @TODO If the process called SYS_setup_callback(SIGNAL, signal_handler) then
            //    we call it first and let process do some termination logic then 500ms (or less)
            //    we kill all threads in the process.
            process->killed = true;
            for (int ti=0;ti<process->threads_len;ti++) {
                EXEC_Thread* thread = &process->threads[ti];
                thread->killed = true;
            }

            // Resources are freed when scheduler detects processes with zero threads.
            printf("Killed #%u %s \n", process->processID, process->elfBaseName);
        }
    }
    
    UNLOCK_INT(&g_process_lock);

    for (int i=CORE_LIMIT-1;i>=0;i--) {
        EXEC_Core* core = &cores[i];
        UNLOCK_INT(&core->thread_lock);
    }

    return foundAny;
}

void EXEC_kill_process(EXEC_Process* process) {
    if(process->killed) {
        // Already killed
        return;
    }
    bool foundAny = false;
    for (int i=0;i<CORE_LIMIT;i++) {
        EXEC_Core* core = &cores[i];
        LOCK_INT(&core->thread_lock);
    }
    
    LOCK_INT(&g_process_lock);

    for (int pi=0;pi<g_processes_len;pi++) {
        EXEC_Process* process = &g_processes[pi];
        // @TODO If the process called SYS_setup_callback(SIGNAL, signal_handler) then
        //    we call it first and let process do some termination logic then 500ms (or less)
        //    we kill all threads in the process.
        for (int ti=0;ti<process->threads_len;ti++) {
            EXEC_Thread* thread = &process->threads[ti];
            thread->killed = true;
        }

        // Resources are freed when scheduler detects processes with zero threads.
        // printf("Killed %s, process=%u\n", process->elfBaseName, process->processID);
    }
    
    UNLOCK_INT(&g_process_lock);

    for (int i=CORE_LIMIT-1;i>=0;i--) {
        EXEC_Core* core = &cores[i];
        UNLOCK_INT(&core->thread_lock);
    }
}



ELOS_Error EXEC_create_user_process(const char* path) {
    ELOS_Error returnValue = ELOS_ERR_UNKNOWN;

    ElfObject object;
    returnValue = read_elf(path, &object);

    if (returnValue != ELOS_OK) {
        return returnValue;
    }

    EXEC_Process* process = create_process();
    if (!process) {
        printf("Could not create user process\n");
        // @TODO Free resources
        goto exit;
    }
    process->permissions = object.permissions;
    
    EXEC_Thread* found_thread = create_thread(process);
    if (!found_thread) {
        printf("Could not create user thread\n");
        // @TODO Free resources
        goto exit;
    }

    // @TODO Distribute to different cores.
    //   We also want thread load balancing on cores.
    //   We also wants thread core affinity and IRQ affinity.
    int coreIndex = CPU_get_core_index();
    
    EXEC_Core* core = &cores[coreIndex];

    LOCK_INT(&core->thread_lock);

    int stack_size = process->permissions->thread.defaultStackSize;
    void* phys_stack = PMEM_alloc_phys(stack_size);
    if (!phys_stack) {
        // @TODO Free resources
        printf("Could not alloc user thread stack\n");
        goto exit;
    }
    void* virt_stack = (void*)(u64)0xF0000000;
    void* kernel_virt_stack = PMEM_phys_to_kernel(phys_stack);
    // Clear with non-zero value, if user has a bug then it may dereference pointer with 9A which gives user a clue about uninitialized memory.
    memset(kernel_virt_stack, 0x9A, stack_size);
    PMEM_map_memory(object.pageTable, virt_stack, phys_stack, stack_size, PMEM_FLAG_USER_SPACE);

    process->pageTable = object.pageTable;

    found_thread->stack_size = stack_size;
    found_thread->stack = virt_stack;
    found_thread->phys_stack = phys_stack;
    found_thread->userSpace = true;
    found_thread->entry = object.virt_entry_point;
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
    frame->rip = (u64)object.virt_entry_point;
    frame->cr3 = (u64)PMEM_kernel_to_phys(process->pageTable);

    // This is so dumb. a page for a tiny little string
    char* name = PMEM_alloc_phys(4096);
    name = PMEM_phys_to_kernel(name);
    char* slashPos = strrchr(path, '/');
    if (slashPos) {
        strncpy(name, slashPos + 1, 4096);
    } else {
        strncpy(name, path, 4096);
    }

    found_thread->process = process;
    process->processID = __atomic_add_fetch(&g_nextProcessID, 1, __ATOMIC_SEQ_CST);
    process->elfBaseName = name;
    
    int free_index = reserve_free_core_thread_index();
    if (free_index == -1) {
        printf("No free core thread index\n");
        // @TODO Free resources
        goto exit;
    }

    core->threads[free_index] = found_thread;

    returnValue = ELOS_OK;

exit:
    // @TODO Cleanup allocated stuff.
    UNLOCK_INT(&core->thread_lock);
    return returnValue;
}
