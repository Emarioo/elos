
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

volatile u32* g_apic_base;



#define APIC_APICID     0x20
#define APIC_APICVER    0x30
#define APIC_TASKPRIOR  0x80
#define APIC_EOI        0x0B0
#define APIC_LDR        0x0D0
#define APIC_DFR        0x0E0
#define APIC_SPURIOUS   0x0F0
#define APIC_ESR        0x280
#define APIC_ICRL       0x300
#define APIC_ICRH       0x310
#define APIC_LVT_TMR	0x320
#define APIC_LVT_PERF	0x340
#define APIC_LVT_LINT0	0x350
#define APIC_LVT_LINT1	0x360
#define APIC_LVT_ERR	0x370
#define APIC_TMRINITCNT	0x380
#define APIC_TMRCURRCNT	0x390
#define APIC_TMRDIV	    0x3E0
#define APIC_LAST	    0x38F
#define APIC_DISABLE	0x10000
#define APIC_SW_ENABLE	0x100
#define APIC_CPUFOCUS	0x200
#define APIC_NMI	 (4<<8)



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

    while (1) {
        int scancode = ps2_poll_scancode();
        if (scancode == 0)
            break;
        int chr = scancode_to_char(scancode, 0);
        printf("scancode %d, %c\n", scancode, chr);
    }

    if (g_apic_base)
        g_apic_base[APIC_EOI/4] = 0; // clear EOI
}


void unused_handler(int isr_number, PageFaultFrame* frame, u64 extra) {
    printf("Interrupt unused #%d\n", isr_number);

    if (g_apic_base)
        g_apic_base[APIC_EOI/4] = 0; // clear EOI
}



void interrupt_timer() {
    printf("Timer triggered, %x\n", g_apic_base);

    if (g_apic_base)
        g_apic_base[APIC_EOI/4] = 0; // clear EOI
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
    if (!acpi_ioapic_address) {
        printf("ACPI tables does not specify IOAPIC address\n");
        return;
    }
    if (!acpi_lapic_address) {
        printf("ACPI tables does not specify LAPIC address\n");
        return;
    }

    asm ( "cli\n" );


    // Mask PIC IRQs
    outb(0x21, 0xFF);
    outb(0xA1, 0xFF);

    #define IA32_APIC_BASE_MSR 0x1B
    #define APIC_SOFTWARE_ENABLE 0x100

    // @TODO Check APIC

    u32 eax,ebx,ecx,edx;
    cpuid(1, 0, &eax,&ebx,&ecx,&edx);

    bool x2apic_support = (ecx & (1<<21)) != 0;
    bool apic_support = (edx & (1<<9)) != 0;

    if (!apic_support) {
        printf("Missing APIC support\n");
        return;
    }

    // printf("APIC support: apic=%d x2apic=%d\n", apic_support, x2apic_support);

    u64 apic_msr = rdmsr(IA32_APIC_BASE_MSR);
    // printf("IA32_APIC_BASE_MSR=%x\n", apic_msr);

    apic_msr = (apic_msr & ~0xFFFFFC00) | acpi_lapic_address | ((u64)1 << 11); // enable APIC, disable x2APIC, keep reserved and BSP bits (BSP = boot processor)
    wrmsr(IA32_APIC_BASE_MSR, apic_msr);
    // printf("Write new: IA32_APIC_BASE_MSR=%x\n", apic_msr);

    u32* apic_base = (void*)acpi_lapic_address;
    g_apic_base = apic_base;

    PMEM_map_memory((void*)apic_base, (void*)apic_base, PAGE_SIZE, PMEM_FLAG_NOT_CACHED);


    // Reset APIC to known state. (doesn't seem necessary in QEMU but very important on real Hardware)
    // u32 tmp;
    // apic_base[APIC_DFR/4] = 0xFFFFFFFF; // reset Destination Format Register
    // tmp = apic_base[APIC_LDR/4];
    // tmp &= 0x00FFFFFF;
    // tmp |= 1;
    // apic_base[APIC_LDR/4] = tmp;
    // apic_base[APIC_LVT_TMR/4] = APIC_DISABLE;
    // apic_base[APIC_LVT_PERF/4] = APIC_NMI;
    // apic_base[APIC_LVT_LINT0/4] = APIC_DISABLE;
    // apic_base[APIC_LVT_LINT1/4] = APIC_DISABLE;
    // apic_base[APIC_TASKPRIOR/4] = 0;


    apic_base[APIC_TMRDIV/4] = 0x3;
    apic_base[APIC_LVT_TMR/4] = 48 | (1 << 17); // periodic mode
    apic_base[APIC_TMRINITCNT/4] = 10000000;

    printf("LVT timer=%x\n", apic_base[APIC_LVT_TMR / 4]);


    int lapic_id = apic_base[APIC_APICID/4];
    printf("apic id: %d\n", lapic_id);

    apic_base[APIC_SPURIOUS/4] = APIC_SOFTWARE_ENABLE | 0xFF;

    printf("SVR: %x\n", apic_base[APIC_SPURIOUS]);


    PMEM_map_memory((void*)acpi_ioapic_address, (void*)acpi_ioapic_address, PAGE_SIZE, PMEM_FLAG_NOT_CACHED);

    // move APIC ID into ioapic here, currently fine since it's zero.
    // cpuWriteIoApic((void*)acpi_ioapic_address, 0x12, 33);
    cpuWriteIoApic((void*)acpi_ioapic_address, 0x12, 33 | (1 << 15));
    cpuWriteIoApic((void*)acpi_ioapic_address, 0x13, 0);

    
    apic_base[APIC_EOI/4] = 0; // clear EOI
    apic_base[APIC_ESR/4] = 0; // clear Error Status
    apic_base[APIC_ESR/4] = 0; // clear Error Status

    asm ( "sti\n" );
}


void CPU_reset() {
    acpi_system_reset();
}

