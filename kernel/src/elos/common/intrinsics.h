#pragma once

#include "elos/common/types.h"


static inline void cli() {
    asm volatile ( "cli\n" );
}

static inline void sti() {
    asm volatile ( "sti\n" );
}


static inline u64 get_rflags() {
    u64 rax;
    asm volatile (
        "pushfq\n"
        "pop %0\n"
        : "=a" (rax)
        :
    );
    return rax;
}


static inline void outb(u16 port, u8 value) {
    asm volatile (
        "outb %0, %1\n"
        :
        : "a" (value), "dN" (port)
    );
}
static inline void outw(u16 port, u16 value) {
    asm volatile (
        "outw %0, %1\n"
        :
        : "a" (value), "dN" (port)
    );
}
static inline void outl(u16 port, u32 value) {
    asm volatile (
        "outl %0, %1\n"
        :
        : "a" (value), "dN" (port)
    );
}

static inline u8 inb(u16 port) {
    u8 value;
    asm volatile (
        "inb %1, %0\n"
        : "=a" (value)
        : "dN" (port)
    );
    return value;
}
static inline u16 inw(u16 port) {
    u16 value;
    asm volatile (
        "inw %1, %0\n"
        : "=a" (value)
        : "dN" (port)
    );
    return value;
}
static inline u32 inl(u16 port) {
    u32 value;
    asm volatile (
        "inl %1, %0\n"
        : "=a" (value)
        : "dN" (port)
    );
    return value;
}

static inline void pause() {
    asm volatile (
        "pause\n"
    );
}

static inline uint64_t rdtsc() {
    uint32_t value_low;
    uint32_t value_high;
    asm volatile (
        "rdtsc\n"
        : "=a" (value_low), "=d" (value_high)
        :
        :
    );
    return (u64)value_low | ((u64)value_high << 32);
}

static inline u64 rdmsr(u32 msr)
{
    u32 low, high;

    asm volatile (
        "rdmsr"
        : "=a"(low), "=d"(high)
        : "c"(msr)
    );

    return ((u64)high << 32) | low;
}

static inline void wrmsr(u32 msr, u64 value)
{
    u32 low = value & 0xFFFFFFFF;
    u32 high = value >> 32;

    asm volatile (
        "wrmsr"
        : 
        : "c"(msr), "a"(low), "d"(high)
    );
}

static inline void cpuid(
    uint32_t leaf,
    uint32_t subleaf,
    uint32_t* eax,
    uint32_t* ebx,
    uint32_t* ecx,
    uint32_t* edx)
{
    uint32_t a, b, c, d;

    asm volatile (
        "cpuid"
        : "=a"(a), "=b"(b), "=c"(c), "=d"(d)
        : "a"(leaf), "c"(subleaf)
    );

    if (eax) *eax = a;
    if (ebx) *ebx = b;
    if (ecx) *ecx = c;
    if (edx) *edx = d;
}

static inline u64 read_cr2() {
    u64 reg;
    asm volatile (
        "mov %%cr2, %0\n"
        : "=r" (reg)
    );
    return reg;
}
static inline u64 read_cr3() {
    u64 reg;
    asm volatile (
        "mov %%cr3, %0\n"
        : "=r" (reg)
    );
    return reg;
}
static inline void write_cr3(u64 reg) {
    asm volatile (
        "mov %0, %%cr3\n"
        :
        : "r" (reg)
    );
}
static inline void flush_tlb_entry(void* addr) {
    asm volatile (
        "invlpg (%0)\n"
        :
        : "r" (addr)
    );
}

static inline void flush_tlb_full() {
    asm volatile (
        "mov cr3, cr3\n"
    );
}

// static inline void flush_tlb_full() {
//     asm volatile (
//         "mov cr3, cr3\n"
//     );
// }
