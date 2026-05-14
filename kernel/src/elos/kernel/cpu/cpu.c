
#include "elos/cpu.h"

#include "elos/common/types.h"
#include "elos/kernel_console.h"

#include "elos/kernel/driver/acpi.h"

#include "elos/common/intrinsics.h"
#include "elos/physical_memory.h"

#include "elos/kernel/kbd/ps2.h"
#include "elos/kernel/kbd/keys.h"



void init_gdt();
void init_idt();
void init_apic();


void CPU_init(BootAPI* boot_api) {

    init_gdt();
    init_idt();

    acpi_init(boot_api);

    init_apic();

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

_align(16)
static IDT_Entry _idt[256];

#define printf(...) KCON_printf(__VA_ARGS__)

typedef struct {
    // uint64_t r11, r10, r9, r8;
    // uint64_t rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t rbp;

    uint64_t error_code;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} PageFaultFrame;

volatile u32* g_lapic;


void exception_handler(int isr_number, PageFaultFrame* frame, u64 extra) {
    if (isr_number == 14) {
        u64 fault_address = read_cr2();
        printf("EXCEPTION #%d (rip=0x%x addr=0x%x err=0x%x)\n", isr_number, frame->rip, fault_address, frame->error_code);
    } else {
        printf("EXCEPTION #%d (error code=0x%x)\nHALTING\n", isr_number, frame->error_code);
    }
    while (1) asm ( "cli\npause\n" );
}

void interrupt_handler(int isr_number, PageFaultFrame* frame, u64 extra) {
    printf("Interrupt #%d\n", isr_number);
    
    int scancode = ps2_read_scancode();
    int chr = scancode_to_char(scancode, 0);

    printf("scancode %d, %c\n", scancode, chr);

    if (g_lapic)
        g_lapic[0xB0/4] = 0; // clear EOI
}


void unused_handler(int isr_number, PageFaultFrame* frame, u64 extra) {
    printf("Interrupt unused #%d\n", isr_number);

    if (g_lapic)
        g_lapic[0xB0/4] = 0; // clear EOI
}



void interrupt_timer() {
    printf("Timer triggered\n");

    if (g_lapic)
        g_lapic[0xB0/4] = 0; // clear EOI
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

void init_gdt() {
    
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

    asm ( "sti\n" );
}

void init_idt() {

    asm ( "cli\n" );

    for (int vector = 0; vector < 256; vector++) {
        // 0x8e = 64-bit interrupt gate, present bit, super privilege
        idt_set_descriptor(vector, isr_stub_table[vector], 0x8e);
        vectors[vector] = true;
    }
    _idt_register.base = (u64)&_idt[0];
    _idt_register.limit = sizeof(*_idt) * IDT_MAX_DESCRIPTORS - 1;

    asm ( "lidt %0\n" : : "m"(_idt_register));

    asm ( "sti\n" );
}



uint32_t cpuReadIoApic(void *ioapicaddr, uint32_t reg)
{
   uint32_t volatile *ioapic = (uint32_t volatile *)ioapicaddr;
   ioapic[0] = (reg & 0xff);
   return ioapic[4];
}

void cpuWriteIoApic(void *ioapicaddr, uint32_t reg, uint32_t value)
{
   uint32_t volatile *ioapic = (uint32_t volatile *)ioapicaddr;
   ioapic[0] = (reg & 0xff);
   ioapic[4] = value;
}




void init_apic() {

    asm ( "cli\n" );


    #define IA32_APIC_BASE_MSR 0x1B
    #define APIC_SOFTWARE_ENABLE 0x100

    // @TODO Check APIC

    // asm (
    //     "cpuid\n"
    // )

    u64 apic_base = rdmsr(IA32_APIC_BASE_MSR);
    apic_base |= (u64)1 << 11; // enable APIC
    wrmsr(IA32_APIC_BASE_MSR, apic_base);

    // Read base from MADT?
    u64 lapic_base = apic_base & 0xFFFFF000;

    PMEM_map_memory((void*)lapic_base, (void*)lapic_base, PAGE_SIZE, PMEM_FLAG_NOT_CACHED);

    volatile u32* lapic = (volatile u32*)lapic_base;

    g_lapic = lapic;

    // lapic[0x3e0/4] = 0x3;
    // lapic[0x320/4] = 48 | (1 << 17);
    // lapic[0x380/4] = 10000000;

    // printf("LVT timer=%x\n", lapic[0x320 / 4]);


    int lapic_id = lapic[0x20/4];
    printf("apic id: %d\n", lapic_id);

    // 0xF0 = offset to spurious vector (/4 because lapic is u32)
    #define SPURIOUS_VECTOR_OFFSET (0xF0/4)
    lapic[SPURIOUS_VECTOR_OFFSET] = (lapic[SPURIOUS_VECTOR_OFFSET] & ~0xFF) | (APIC_SOFTWARE_ENABLE | 0xFF);

    printf("SVR: %x\n", lapic[SPURIOUS_VECTOR_OFFSET]);

    // Mask PIC IRQs
    outb(0x21, 0xFF);
    outb(0xA1, 0xFF);

    // Read from MADT
    #define IOAPIC_BASE ((void*)0xFEC00000)
    
    PMEM_map_memory((void*)IOAPIC_BASE, (void*)IOAPIC_BASE, PAGE_SIZE, PMEM_FLAG_NOT_CACHED);

    // move APIC ID into ioapic here, currently fine since it's zero.
    cpuWriteIoApic(IOAPIC_BASE, 0x12, 33);
    cpuWriteIoApic(IOAPIC_BASE, 0x13, 0);

    
    lapic[0xB0/4] = 0; // clear EOI
    lapic[0x280/4] = 0; // clear Error Status
    lapic[0x280/4] = 0; // clear Error Status

    asm ( "sti\n" );
}
