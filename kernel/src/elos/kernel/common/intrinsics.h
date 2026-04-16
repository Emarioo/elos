#pragma once

#include "elos/kernel/common/types.h"

static inline void outb(u16 port, u8 value) {
    asm(
        "outb %0, %1\n"
        :
        : "a" (value), "dN" (port)
    );
}
static inline void outw(u16 port, u16 value) {
    asm(
        "outw %0, %1\n"
        :
        : "a" (value), "dN" (port)
    );
}
static inline void outl(u16 port, u32 value) {
    asm(
        "outl %0, %1\n"
        :
        : "a" (value), "dN" (port)
    );
}

static inline u8 inb(u16 port) {
    u8 value;
    asm(
        "inb %1, %0\n"
        : "=a" (value)
        : "dN" (port)
    );
    return value;
}
static inline u16 inw(u16 port) {
    u16 value;
    asm(
        "inw %1, %0\n"
        : "=a" (value)
        : "dN" (port)
    );
    return value;
}
static inline u32 inl(u16 port) {
    u32 value;
    asm(
        "inl %1, %0\n"
        : "=a" (value)
        : "dN" (port)
    );
    return value;
}
