#pragma once

#include "elos/boot_api.h"

#include "elos/common/types.h"


#define KERNEL_CODE_SEGMENT 0x8
#define KERNEL_DATA_SEGMENT 0x10
// Theoretical segment you might want, aligns nicely with syscall/sysret
#define USER_CODE_COMPATIBILITY_SEGMENT   0x18
#define USER_DATA_SEGMENT   0x20
#define USER_CODE_SEGMENT   0x28
#define TASK_STATE_SEGMENT  0x30
#define LAST_SEGMENT TASK_STATE_SEGMENT
// task state takes up two slots

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

    u64 rip;
    u64 cs;
    u64 rflags;
    u64 rsp;
    u64 ss;
} InterruptFrame;


void CPU_init(BootAPI* boot_api);

void CPU_enable_extensions();

void CPU_enable_interrupt();
void CPU_disable_interrupt();

void CPU_reset();

void CPU_sleep(u64 nanoseconds);

int CPU_get_core_index();
int CPU_get_core_count();

u64 CPU_tsc_per_sec();

void CPU_start_core(u32 apic_id);




void LOCK(volatile u32* ptr);
void UNLOCK(volatile u32* ptr);
bool IS_LOCKED(volatile u32* ptr);


void LOCK_INT(volatile u32* ptr);
void UNLOCK_INT(volatile u32* ptr);
