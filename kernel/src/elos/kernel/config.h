#pragma once

extern unsigned char __kernel_start[];
extern unsigned char __kernel_end[];
extern unsigned char __stack_start[];
extern unsigned char __stack_end[];


// These addresses exist in kernel's linker script (kernel/sections.ld)
// Kernel itself knows them with the above declarations.
// EFI applications do not and use these:

#define VIRTUAL_KERNEL_BASE 0xFFFFFFFF80000000
#define __KERNEL_START ((void*)0x200000)
#define __KERNEL_END   ((void*)(0x200000 + 0x200000))
