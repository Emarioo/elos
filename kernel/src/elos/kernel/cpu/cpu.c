
#include "elos/cpu.h"

#include "elos/common/types.h"
#include "elos/kernel_console.h"

#include "elos/kernel/driver/acpi.h"



void init_gdt_idt();


void CPU_init(BootAPI* boot_api) {

    init_gdt_idt();

    acpi_init(boot_api);

}


#pragma pack(push, 1)
typedef struct GDT_Register {
    u16 limit;
    u64 base;
} GDT_Register;
#pragma pack(pop)


#pragma pack(push, 1)
typedef struct IDT_Register {
    u16 limit;
    u64 base;
} IDT_Register;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct IDT_Entry {
    u16 isr_low;
    u16 kernel_cs;
    u8  ist;
    u8  attributes;
    u16 isr_mid;
    u32 isr_high;
    u32 _reserved;
} IDT_Entry;
#pragma pack(pop)

static GDT_Register _gdt_register;
static IDT_Register _idt_register;

static u64 _gdt[3];

__attribute__((aligned(0x10)))
static IDT_Entry _idt[256];

#define printf(...) KCON_printf(__VA_ARGS__)

void exception_handler(int vector, int error_code) {
    printf("EXCEPTION #%d (error code %d)\nHALTING\n", vector, error_code);
    while (1) asm ( "cli\n" );
}




#define MAKE_SEGMENT_DESC(BASE,LIMIT,ACCESS_BYTE,FLAGS) (\
        (((u64)(BASE) & 0xFF000000) << 32) | (((u64)(BASE) & 0xFF0000) << 16) | (((u64)(BASE) & 0xFFFF) << 16) | \
        (((u64)(LIMIT) & 0xF0000) << 32) | (((u64)(LIMIT) & 0xFFFF) << 0) | \
        (((u64)(ACCESS_BYTE) & 0xFF) << 40) | \
        (((u64)(FLAGS) & 0xF) << 52) \
    )

#define MB 0x100000

#define IDT_MAX_DESCRIPTORS 256

static bool vectors[IDT_MAX_DESCRIPTORS];

extern void* isr_stub_table[];

// offset to Kernel code descriptor, set int init_gdb_idt
#define GDT_OFFSET_KERNEL_CODE 8

void idt_set_descriptor(uint8_t vector, void* isr, uint8_t flags) {
    IDT_Entry* descriptor = &_idt[vector];

    descriptor->isr_low        = (uint64_t)isr & 0xFFFF;
    descriptor->kernel_cs      = GDT_OFFSET_KERNEL_CODE;
    descriptor->ist            = 0;
    descriptor->attributes     = flags;
    descriptor->isr_mid        = ((uint64_t)isr >> 16) & 0xFFFF;
    descriptor->isr_high       = ((uint64_t)isr >> 32) & 0xFFFFFFFF;
    descriptor->_reserved       = 0;
}

extern u8 __text_start;
extern u8 __data_start;

void init_gdt_idt() {
    
    asm ( "cli\n" );

    // Base and LIMIT are set to zero because they are ignored in 64-bit mode.

    // Null descriptor.
    _gdt[0] = MAKE_SEGMENT_DESC(0,0,0,0);
    // Kernel code descriptor
    _gdt[1] = MAKE_SEGMENT_DESC(0, 0, 0x9A, 0xA); // executable and readable bit, long mode code flag VERY IMPORTANT FLAG,
                                                  // page granularity for limit field (ignored since 64-bit mode?), 
    // Kernel data descriptor
    _gdt[2] = MAKE_SEGMENT_DESC(0, 0, 0x92, 0xC); // writable bit, page granularity for limit field (ignored since we are 64-bit mode?)
    // @TODO User mode descriptor
    // @TOD Task descriptor? if needed?
    // _gdt[3] = MAKE_SEGMENT_DESC(0, 0, 0x89, 0);

    
    _gdt_register.limit = sizeof(_gdt) - 1;
    _gdt_register.base = (u64)&_gdt;
    
    asm ( "lgdt %0\n" : : "m" (_gdt_register) );

    asm volatile (
        "mov $0x10, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%ss\n"
        "pushq $0x08\n"
        "leaq 1f(%%rip), %%rax\n"
        "pushq %%rax\n"
        "lretq\n"
        "1:\n"
        :::"rax"
    );


    for (int vector = 0; vector < 32; vector++) {
        idt_set_descriptor(vector, isr_stub_table[vector], 0x8e);
        vectors[vector] = true;
    }
    _idt_register.base = (u64)&_idt[0];
    _idt_register.limit = sizeof(*_idt) * IDT_MAX_DESCRIPTORS - 1;

    asm ( "lidt %0\n" : : "m"(_idt_register));

    asm ( "sti\n" );
}
