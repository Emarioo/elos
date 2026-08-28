#pragma once

#include "elos/boot_api.h"

#include "elos/common/types.h"


//#################
//      TYPES
//#################

#define KERNEL_CODE_SEGMENT 0x8
#define KERNEL_DATA_SEGMENT 0x10
// Theoretical segment you might want, aligns nicely with syscall/sysret
#define USER_CODE_COMPATIBILITY_SEGMENT  0x18
#define USER_DATA_SEGMENT   0x20
#define USER_CODE_SEGMENT   0x28
#define USER_DATA_COMPATIBILITY_SEGMENT  0x30
#define TASK_STATE_SEGMENT  0x38
#define LAST_SEGMENT TASK_STATE_SEGMENT
// task state takes up two slots

#define TIMER_FREQUENCY_NS 1000000 // 1 ms

typedef struct {
    u64 reserved; // here to align struct

    // @TODO GS,FS base
    u64 fs;
    u64 gs;
    u64 cr3;

    // @TODO Save float, more?
    u64 xmm[16];

    u64 r15;
    u64 r14;
    u64 r13;
    u64 r12;
    u64 r11;
    u64 r10;
    u64 r9;
    u64 r8;
    u64 rdi;
    u64 rsi;
    u64 rdx;
    u64 rcx;
    u64 rbx;
    u64 rax;
    u64 rbp;

    union {
        struct {
            u64 rip;
            u64 cs;
            u64 rflags;
            u64 rsp;
            u64 ss;
        };
        struct {
            u32 eip;
            u32 cs;
            u32 eflags;
            u32 esp;
            u32 ss;
        } compat;
    };
} ContextFrame;


typedef struct {
    uint64_t r11, r10, r9, r8;
    uint64_t rdi, rsi, rdx, rcx, rax;

    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} InterruptFrame;

typedef void(*FN_interrupt_handler)(u32 vector, InterruptFrame* frame);



//#####################
//      FUNCTIONS
//#####################



void CPU_init(BootAPI* boot_api);
void CPU_reset();

// Spin sleep should be used in early booting.
// Use EXEC_sleep when scheduler is initialized.
void CPU_spin_sleep(u64 nanoseconds);

u64 CPU_ticks_per_second();
u64 CPU_ticks();

int CPU_get_core_index();
int CPU_get_core_count();


void CPU_enable_interrupt();
void CPU_disable_interrupt();

void CPU_set_irq(u32 coreId, u32 local_irq, u32 global_irq, FN_interrupt_handler handler);

void CPU_get_msi_irq(u32 coreId, u32 local_irq, FN_interrupt_handler handler, u64* messageAddress, u16* messageData);



extern u64 g_timer_frequency_ns;
void CPU_schedule_timer_interrupt(u64 nanoseconds);


void timer_isr();
void timer_isr_ret32();


// @TODO Move sync primitives elsewhere.

void LOCK(volatile u32* ptr);
void UNLOCK(volatile u32* ptr);
bool IS_LOCKED(volatile u32* ptr);

void LOCK_INT(volatile u32* ptr);
void UNLOCK_INT(volatile u32* ptr);
