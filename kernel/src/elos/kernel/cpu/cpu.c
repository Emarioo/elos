
#include "elos/cpu.h"

#include "elos/common/types.h"
#include "elos/common/string.h"
#include "elos/common/intrinsics.h"

#include "elos/kernel_console.h"

#include "elos/kernel/driver/acpi.h"

#include "elos/physical_memory.h"

#include "elos/kernel/kbd/ps2.h"
#include "elos/kernel/kbd/keys.h"



#include "elos/execution.h"

#define printf(...) KCON_printf(__VA_ARGS__)


void init_gdt();
void init_idt();
void init_apic();

u64 tsc_per_second;

void CPU_init(BootAPI* boot_api) {

    init_gdt();
    init_idt();

    acpi_init(boot_api);

    init_apic();

    // CPU_calibrate_tsc();
    // printf("Calibrated tsc: %d/ms\n", tsc_per_second/1000);

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

GDT_Register _gdt_register;
static IDT_Register _idt_register;

static u64 _gdt[3];

_align(16)
static IDT_Entry _idt[256];


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

volatile u32* g_lapic_base;


void ap_trampoline(); // defined in assembly

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

void hpet_calibrate();

void exception_handler(int isr_number, PageFaultFrame* frame, u64 extra) {
    if (isr_number == 14) {
        u64 fault_address = read_cr2();
        printf("EXCEPTION #%d (rip=0x%x addr=0x%x err=0x%x)\n", isr_number, frame->rip, fault_address, frame->error_code);
    } else {
        printf("EXCEPTION #%d (error code=0x%x, rip=%x)\nHALTING\n", isr_number, frame->error_code, frame->rip);
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

    if (g_lapic_base)
        g_lapic_base[APIC_EOI/4] = 0; // clear EOI
}


void unused_handler(int isr_number, PageFaultFrame* frame, u64 extra) {
    printf("Interrupt unused #%d\n", isr_number);

    if (g_lapic_base)
        g_lapic_base[APIC_EOI/4] = 0; // clear EOI
}



// void interrupt_timer() {
//     printf("[TIMER]\n");

//     EXEC_interrupt();

//     if (g_lapic_base)
//         g_lapic_base[APIC_EOI/4] = 0; // clear EOI
// }


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

    // Ensure the macros KERNEL_CODE_SEGMENT, KERNEL_DATA_SEGMENT
    // Match with below.

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

    cli();

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
    g_lapic_base = apic_base;

    PMEM_map_memory((void*)apic_base, (void*)apic_base, PAGE_SIZE, PMEM_FLAG_NOT_CACHED);


    // Reset APIC to known state. (doesn't seem necessary)
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

    apic_base[APIC_EOI/4] = 0; // clear EOI
    apic_base[APIC_ESR/4] = 0; // clear Error Status
    apic_base[APIC_ESR/4] = 0; // clear Error Status

    apic_base[APIC_TMRDIV/4] = 0x3;
    apic_base[APIC_LVT_TMR/4] = 48 | (1 << 17); // periodic mode
    apic_base[APIC_TMRINITCNT/4] = 10000000;

    // printf("LVT timer=%x\n", apic_base[APIC_LVT_TMR / 4]);


    int lapic_id = apic_base[APIC_APICID/4] >> 24;
    // printf("apic id: %d\n", lapic_id >> 24);

    apic_base[APIC_SPURIOUS/4] = APIC_SOFTWARE_ENABLE | 0xFF;

    // printf("SVR: %x\n", apic_base[APIC_SPURIOUS]);


    PMEM_map_memory((void*)acpi_ioapic_address, (void*)acpi_ioapic_address, PAGE_SIZE, PMEM_FLAG_NOT_CACHED);

    // move APIC ID into ioapic here, currently fine since it's zero.
    cpuWriteIoApic((void*)acpi_ioapic_address, 0x12, 33 | (1 << 15)); // 1<<15 does level trigger instead of edge, seems to work better?
    cpuWriteIoApic((void*)acpi_ioapic_address, 0x13, 0);
    
    // Map IRQ2 to Vector #32
    // Use for HPET
    cpuWriteIoApic((void*)acpi_ioapic_address, 0x14, 34); // wants edge trigger
    cpuWriteIoApic((void*)acpi_ioapic_address, 0x15, 0);

    sti();

    // while (1) pause();

    hpet_calibrate();
}

u64 hpet_rdtsc_value;

void hpet_calibrate() {
    cli();

    const HPET_Capability_Register capabilities = *(HPET_Capability_Register*)(acpi_hpet_address + 0x0);
    volatile u64* configuration  = (u64*)(acpi_hpet_address + 0x10);
    // volatile HPET_Configuration_Register* _configuration  = (HPET_Configuration_Register*)(acpi_hpet_address + 0x10);
    volatile u64* interrupt_status = (u64*)(acpi_hpet_address + 0x20);
    volatile u64* main_counter     = (u64*)(acpi_hpet_address + 0xF0);

    volatile u64* timer0 = (u64*)(acpi_hpet_address + 0x100 + 0x20*0);
    volatile HPET_Timer_Register* _timer0 = (HPET_Timer_Register*)(acpi_hpet_address + 0x100 + 0x20*0);
    // volatile HPET_Timer_Register* timer1 = (HPET_Timer_Register*)(acpi_hpet_address + 0x100 + 0x20*1);
    
    volatile u64* comp0 = (u64*)(acpi_hpet_address + 0x108 + 0x20*0);
    // volatile u64* comp1 = (u64*)(acpi_hpet_address + 0x108 + 0x20*1);

    // @TODO Warn if timer is 32-bit    
    
    int irq_number = 2;
    while (irq_number < 32) {
        *timer0 = (irq_number << 9) | (1 << 2);
        //  ->int_route_cnf = irq_number; // IRQ2, hopefully
        // timer0->int_enb_cnf = 1;

        if (((*timer0 >> 9) & 0x3F) != irq_number) {
            printf("Could not set IRQ%d for HPET timer0\n", irq_number);
            irq_number++;
            continue;
        }
        break;
    }

    // 1 second
    u64 delay = (1000000000LU*1000000LU)/capabilities.counter_clk_period;

    *comp0 = *main_counter + delay;

    sti();

    u64 start = rdtsc();
    *configuration = (*configuration & ~2LU) | 1;


    printf("HPET setup, %d\n", *main_counter);

    while (hpet_rdtsc_value == 0) {
        pause();
    }
    u64 diff = hpet_rdtsc_value - start;
    printf("HPET measured: %d M/s\n", diff/1000000);
}
void hpet_isr() {
    hpet_rdtsc_value = rdtsc();
    printf("Triggered %d\n", hpet_rdtsc_value/1000);
}


void CPU_reset() {
    acpi_system_reset();
}


void CPU_calibrate_tsc() {
    cli();

    #define PIT_CMD_PORT 0x43
    #define PIT_DATA_PORT 0x40

    // This is awful calibration. Use HPET or interrupts or something.
    // Found this on https://wiki.osdev.org/Symmetric_Multiprocessing
    // but I think I misunderstood it.
    // From eye measurement it seems to be off by a factor of 2-3 in QEMU and ~1000 on laptop.

    // u64 start = rdtsc();

    // outb(PIT_CMD_PORT, 0x30);
    // outb(PIT_DATA_PORT, 0xA9);
    // outb(PIT_DATA_PORT, 0x4);

    // outb(PIT_CMD_PORT, 0xE2);
    
    // while ((inb(PIT_DATA_PORT) & 0x80) == 0) pause();

    // u64 end = rdtsc();

    sti();

    // tsc_per_second = (end - start) * 1000;
    tsc_per_second = 1000000000;
}


void CPU_sleep(u64 nanoseconds) {
    // @TODO Might be some overflow issues if you sleep for 10 seconds.
    u64 target = (nanoseconds * tsc_per_second)/1000000000  + rdtsc();

    while(1) {
        u64 now = rdtsc();
        if (now >= target)
            break;
        pause();
    }
}

int CPU_get_core_index() {
    return g_lapic_base[APIC_APICID/4] >> 24;
}


void apic_clear_eoi() {
    g_lapic_base[APIC_EOI/4] = 0; // clear EOI
}


void CPU_enable_interrupt() {
    sti();
}
void CPU_disable_interrupt() {
    cli();
}


// MUST BE PAGE ALIGNED. Address is hardcoded in trampoline assembly.
#define TRAMPOLINE_ADDRESS ((void*)0x8000)


void CPU_start_core(u32 apic_id, InterruptFrame* frame) {
    bool mapped = PMEM_map_memory(TRAMPOLINE_ADDRESS, TRAMPOLINE_ADDRESS, PAGE_SIZE, PMEM_FLAG_NONE);
    if (!mapped) {
        printf("Could not map AP trampoline\n");
        return;
    }

    for (int i=0;i<10;i++) {
        printf("Waiting %d\n", i);
        CPU_sleep(1000000000);
    }

    // This code will freeze if APIC ID doesn't exist.

    memcpy(TRAMPOLINE_ADDRESS, ap_trampoline, PAGE_SIZE);

    CPU_disable_interrupt();

    g_lapic_base[APIC_ICRH/4] = (g_lapic_base[APIC_ICRH/4] & 0x00FFFFFF) | (apic_id << 24);
    g_lapic_base[APIC_ICRL/4] = (g_lapic_base[APIC_ICRL/4] & 0xFFF00000) | (0x0C500); // send INIT, Level and Trigger Mode bit is set.
    // Intel manual says trigger mode should be 0 for all but INIT de-assert. We use INIT assert so maybe we should ty 0x004500

    while (g_lapic_base[APIC_ICRL/4] & 0x1000) pause(); // wait for delivery

    g_lapic_base[APIC_ICRH/4] = (g_lapic_base[APIC_ICRH/4] & 0x00FFFFFF) | (apic_id << 24);
    g_lapic_base[APIC_ICRL/4] = (g_lapic_base[APIC_ICRL/4] & 0xFFF00000) | (0x08500); // send INIT deassert, Trigger Mode bit is set.
    
    while (g_lapic_base[APIC_ICRL/4] & 0x1000) pause(); // wait for delivery
    
    CPU_sleep(10000000); // 10ms

    for (int j = 0; j < 2; j++) {
        g_lapic_base[APIC_ESR/4] = 0;
        g_lapic_base[APIC_ICRH/4] = (g_lapic_base[APIC_ICRH/4] & 0x00FFFFFF) | (apic_id << 24);
        g_lapic_base[APIC_ICRL/4] = (g_lapic_base[APIC_ICRL/4] & 0xFFF0F800) | (0x00600) | ((u32)(u64)TRAMPOLINE_ADDRESS/PAGE_SIZE); // send STARTUP IPI
        
        CPU_sleep(200000); // 200us

        while (g_lapic_base[APIC_ICRL/4] & 0x1000) pause(); // wait for delivery
    }

    CPU_enable_interrupt();
}

_align(4096) u8  initial_ap_stack[32 * 0x1000]; // 4K stack for each AP. They can setup more later.
_align(4096) u32 initial_ap_stack_top;

// ap = Application Processor, BSP = Boot processor?
void ap_entry(int id) {
    

    int lapic_id = g_lapic_base[APIC_APICID/4] >> 24;
    // while (1) pause();

    printf("AP #%d started (edi=%d)\n", lapic_id, id);

    while (1) pause();
}
