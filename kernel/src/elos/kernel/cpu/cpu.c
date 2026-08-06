
#include "elos/cpu.h"

#include "elos/common/types.h"
#include "elos/common/string.h"
#include "elos/common/intrinsics.h"

#include "elos/kernel_console.h"
#include "elos/system_console.h"

#include "elos/kernel/driver/acpi.h"

#include "elos/physical_memory.h"

#include "elos/kernel/eve/ps2.h"
#include "elos/kernel/eve/keys.h"



#include "elos/execution.h"

#define printf(...) KCON_printf(__VA_ARGS__)


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


#define MSR_GS_BASE        0xC0000101
#define MSR_KERNEL_GS_BASE 0xC0000102

#define MSR_IA32_EFER 0xC0000080


void ap_trampoline(); // defined in assembly


void init_gdt();
void init_idt();
void init_apic();
void init_syscall();
void calibrate_tsc();

// MUST BE PAGE ALIGNED. Address is hardcoded in trampoline assembly.
#define TRAMPOLINE_ADDRESS ((void*)0x8000)

uint32_t cpuReadIoApic(void *ioapicaddr, uint32_t reg);
void cpuWriteIoApic(void *ioapicaddr, uint32_t reg, uint32_t value);

volatile u32* g_lapic_base;

u64 tsc_per_sec;

u64 CPU_tsc_per_sec() {
    // @TODO Should be per CORE
    return tsc_per_sec;
}

static void disable_pit() {
    outb(0x43, 0x30);
    outb(0x40, 0);
    outb(0x40, 0);
    outb(0x21, 0xFF);
    outb(0xA1, 0xFF);
}

void CPU_init(BootAPI* boot_api) {
    
    init_gdt();
    init_idt();

    // Disable PIT (gives out some stray IRQ2 in the beginning which interfers with HPET interrupt)
    disable_pit();

    acpi_init(boot_api);

    g_lapic_base = (void*)acpi_lapic_address;

    init_apic();
    init_syscall();



    // Enable IRQ1 for keyboard interrupts for core 0 (apic id = 0)
    cpuWriteIoApic((void*)acpi_ioapic_array[0].address, 0x12, 33 | (1 << 15)); // 1<<15 does level trigger instead of edge, seems to work better?
    cpuWriteIoApic((void*)acpi_ioapic_array[0].address, 0x13, 0);

    // @TODO Calibration per core
    calibrate_tsc();


    bool mapped = PMEM_map_memory(g_kernelPageTable, TRAMPOLINE_ADDRESS, TRAMPOLINE_ADDRESS, PAGE_SIZE, PMEM_FLAG_NONE);
    if (!mapped) {
        printf("Could not map AP trampoline\n");
        return;
    }
    memcpy(TRAMPOLINE_ADDRESS, ap_trampoline, PAGE_SIZE);

    int lapic_id = g_lapic_base[APIC_APICID/4] >> 24;

    // Starting cores requires some sleep and precise timings.
    // We must calibrate TSC first.
    for (int i=0;i<acpi_lapic_ids_len;i++) {
        u32 apic_id = acpi_lapic_ids[i];
        if (apic_id == lapic_id)
            continue; // don't start yourself

        // printf("APIC id: %d\n", apic_id);
        CPU_start_core(apic_id);
    }
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

#pragma pack(push, 1)
typedef struct {
	uint32_t _reserved0;
	uint64_t rsp0;
	uint64_t rsp1;
	uint64_t rsp2;
	uint64_t _reserved1;
	uint64_t ist1;
	uint64_t ist2;
	uint64_t ist3;
	uint64_t ist4;
	uint64_t ist5;
	uint64_t ist6;
	uint64_t ist7;
	uint64_t _reserved2;
	uint16_t _reserved3;
	uint16_t iomap_base;
} TSS_Entry;
#pragma pack(pop)


_align(8) GDT_Register _gdt_register[CORE_LIMIT];
_align(8) IDT_Register _idt_register; // Same for every core
_align(16) TSS_Entry   _tss_entry[CORE_LIMIT];

static u64 _gdt[CORE_LIMIT][(LAST_SEGMENT+16)/8]; // +16 for NULL and the width of the last segment


_align(16)
static IDT_Entry _idt[256];


typedef struct {
    uint64_t r11, r10, r9, r8;
    uint64_t rdi, rsi, rdx, rcx, rax;

    uint64_t error_code;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} PageFaultFrame;


typedef struct {
    uint64_t r11, r10, r9, r8;
    uint64_t rdi, rsi, rdx, rcx, rax;

    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} KeyboardInterruptFrame;

typedef struct {
    u64 rip;
    u64 cs;
    u64 rflags;
    u64 rsp;
    u64 ss;
} IretFrame;



void exception_handler(int isr_number, PageFaultFrame* frame, u64 extra) {
    write_cr3((u64)g_kernelPageTable);

    int coreIndex = CPU_get_core_index(); // a little dangerous if APIC address caused fault

    if (isr_number == 14) {
        u64 fault_address = read_cr2();
        printf("EXCEPTION #%d (rip=0x%x err=0x%x core=%d rsp=0x%x addr=0x%x)\n", isr_number, frame->rip, frame->error_code, coreIndex, frame->rsp, fault_address);
    } else {
        printf("EXCEPTION #%d (rip=0x%x err=0x%x core=%d rsp=0x%x)\n", isr_number, frame->rip, frame->error_code, coreIndex, frame->rsp);

        IretFrame* iretFrame = (void*)((char*)frame + 8 + sizeof(PageFaultFrame));
        printf(" rip=%x\n", iretFrame->rip);
        printf(" cs=%x\n", iretFrame->cs);
        printf(" rflags=%x\n", iretFrame->rflags);
        printf(" rsp=%x\n", iretFrame->rsp);
        printf(" ss=%x\n", iretFrame->ss);

    }
    while (1) asm ( "cli\nhlt\n" );
}

extern Keymap* g_currentKeymap;


// SUPER key cannot be detected when running ELOS in QEMU on Windows.
// Works fine on Linux.
// ELOS_Keycode g_superKey = ELOSKEY_LEFT_SUPER;
ELOS_Keycode g_superKey = ELOSKEY_LEFT_ALT;
bool g_superKeyIsDown = false;

void keyboard_handler(int isr_number, KeyboardInterruptFrame* frame, u64 extra) {
    // printf("Interrupt #%d\n", isr_number);

    u64 userPageTable = read_cr3();

    write_cr3((uintptr_t)g_kernelPageTable);
    
    while (1) {
        int pressed;
        int scancode = ps2_poll_scancode(&pressed);
        if (scancode == 0)
            break;
        
        // @TODO Reboot key is nice but it should not be here.
        int keycode = scancode_to_keycode(g_currentKeymap, scancode);
        if (keycode == ELOSKEY_F1) {
            CPU_reset();
        }

        if (keycode == g_superKey) {
            g_superKeyIsDown = pressed;
        }
        // printf("%d=%d %d %d %d scan=%d\n", keyEvent.keycode, g_superKey, keyEvent.pressed, keyEvent.mods, g_superKeyIsDown, keyEvent.scancode);
        if (keycode == ELOSKEY_T && pressed && g_superKeyIsDown) {
            SCON_enable(!SCON_is_enabled());
            continue;
        }
        
        KBD_push_key_event(scancode, pressed);
        
        // printf("scancode %d, %c\n", scancode, chr);
    }

    write_cr3(userPageTable);
    
    // SS privilege gets cleared on my laptop.
    // May have set up something bad in descriptors.
    // But this ensures we get the right wrong.
    frame->ss |= frame->cs & 3;

    if (g_lapic_base)
        g_lapic_base[APIC_EOI/4] = 0; // clear EOI
}


void unused_handler(int isr_number, PageFaultFrame* frame, u64 extra) {
    printf("Interrupt unused #%d\n", isr_number);

    if (g_lapic_base)
        g_lapic_base[APIC_EOI/4] = 0; // clear EOI
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


void idt_set_descriptor(uint8_t vector, void* isr, uint8_t flags) {
    IDT_Entry* descriptor = &_idt[vector];

    descriptor->isr_low        = (uint64_t)isr & 0xFFFF;
    descriptor->kernel_cs      = KERNEL_CODE_SEGMENT;
    descriptor->ist            = 0;
    descriptor->attributes     = flags;
    descriptor->isr_mid        = ((uint64_t)isr >> 16) & 0xFFFF;
    descriptor->isr_high       = ((uint64_t)isr >> 32) & 0xFFFFFFFF;
    descriptor->_reserved       = 0;
}

extern u8 __text_start;
extern u8 __data_start;

u8 tss_stack_space[CORE_LIMIT][0x1000];

void init_gdt() {
    
    asm ( "cli\n" );

    for (int coreIndex=0;coreIndex<CORE_LIMIT;coreIndex++) {
        _tss_entry[coreIndex].rsp0 = (u64)&tss_stack_space[coreIndex] + sizeof(tss_stack_space[coreIndex]);

        // Null descriptor.
        _gdt[coreIndex][0] = 0;

        #define GDT_PRESENT             0x80
        #define GDT_CODE_OR_DATA        0x10
        #define GDT_TSS                 0x00
        #define GDT_RING0               0x00
        #define GDT_RING3               0x60
        #define GDT_EXEC_READ           0xA
        #define GDT_WRITE_READ          0x2
        #define GDT_TYPE_TSS_AVAILABLE  0x9
        #define GDT_32_BIT_PROTECTED    0x4

        #define GDT_LONG_MODE   0x2
        
        // Base and LIMIT are set to zero because they are ignored in 64-bit mode.

        _gdt[coreIndex][KERNEL_CODE_SEGMENT/8] = MAKE_SEGMENT_DESC(0, 0,
            GDT_PRESENT|GDT_CODE_OR_DATA|GDT_RING0|GDT_EXEC_READ, GDT_LONG_MODE);
        
        _gdt[coreIndex][KERNEL_DATA_SEGMENT/8] = MAKE_SEGMENT_DESC(0, 0,
            GDT_PRESENT|GDT_CODE_OR_DATA|GDT_RING0|GDT_WRITE_READ, 0);
        
        // THIS IS NOT USED but here for completeness? base/limit needs to be set for correctness.
        _gdt[coreIndex][USER_CODE_COMPATIBILITY_SEGMENT/8] = MAKE_SEGMENT_DESC(0, 0,
            GDT_PRESENT|GDT_CODE_OR_DATA|GDT_RING3|GDT_EXEC_READ, GDT_32_BIT_PROTECTED);

        _gdt[coreIndex][USER_DATA_SEGMENT/8] = MAKE_SEGMENT_DESC(0, 0,
            GDT_PRESENT|GDT_CODE_OR_DATA|GDT_RING3|GDT_WRITE_READ, 0);

        _gdt[coreIndex][USER_CODE_SEGMENT/8] = MAKE_SEGMENT_DESC(0, 0,
            GDT_PRESENT|GDT_CODE_OR_DATA|GDT_RING3|GDT_EXEC_READ, GDT_LONG_MODE);

        _gdt[coreIndex][TASK_STATE_SEGMENT/8] = MAKE_SEGMENT_DESC(&_tss_entry[coreIndex], sizeof(_tss_entry[coreIndex])-1,
            GDT_PRESENT|GDT_TSS|GDT_TYPE_TSS_AVAILABLE, 0);

        _gdt[coreIndex][TASK_STATE_SEGMENT/8 + 1] = 0; // @TODO _tss_entry exists in low 32-bit addess space so the "higher" part of TSS can just be zero.

        // User data and code segment are in this order because of sysret loading CS = STAR 63:48 +16 and SS = STAR 63:48 +8

        
        _gdt_register[coreIndex].limit = sizeof(_gdt[coreIndex]) - 1;
        _gdt_register[coreIndex].base = (u64)&_gdt[coreIndex];
    }
    
    int currentCoreIndex = CPU_get_core_index();

    asm ( "lgdt %0\n" : : "m" (_gdt_register[currentCoreIndex]) );

    // Load new descriptors.
    // We hardcode these in the inline assembly:
    //   0x8  = KERNEL_CODE_SEGMENT
    //   0x10 = KERNEL_DATA_SEGMENT
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
    
    // Hardcoded values:
    //   0x30 = TASK_STATE_SEGMENT
    asm (
        "mov $0x30, %%ax\n"
        "ltr %%ax\n"
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

void init_syscall() {
    // Setup syscalls
    // @TODO Check cpuid
    

    #define MSR_STAR 0xc0000081
    #define MSR_LSTAR 0xc0000082
    #define MSR_CSTAR 0xc0000083
    #define MSR_SFMASK 0xc0000084

    // @TODO data segment must be CS+8 in GDT. syscall expects this.
    //    Similar restriction for user land CS and SS.
    //    (add assert for this)

    // We assume that
    //   userland SS = USER_CODE_COMPATIBILITY_SEGMENT+8
    //   userland CS = USER_CODE_COMPATIBILITY_SEGMENT+16
    //   32-bit compatiblity CS = USER_CODE_COMPATIBILITY_SEGMENT
    u64 star = ((u64)(KERNEL_CODE_SEGMENT) << 32)
        | ((u64)(USER_CODE_COMPATIBILITY_SEGMENT) << 48);
    u64 lstar = (u64)syscall_handler;
    u64 sfmask = 0x200LU; // Clears interrupt enable flag.
    wrmsr(MSR_STAR,   star);
    wrmsr(MSR_LSTAR,  lstar);
    wrmsr(MSR_SFMASK, sfmask);

    u64 efer = rdmsr(MSR_IA32_EFER);
    efer |= 1; // SYSCALL ENABLE (for 64-bit intel)
    wrmsr(MSR_IA32_EFER, efer);

}

void init_apic() {
    if (!acpi_ioapic_array_len) {
        printf("ACPI tables does not specify IOAPIC address\n");
        return;
    }
    if (!acpi_lapic_address) {
        printf("ACPI tables does not specify LAPIC address\n");
        return;
    }

    
    cli();


    int coreIndex = CPU_get_core_index();
    EXEC_Core* core = &cores[coreIndex];
    core->syscall_stack_size = 0x4000;
    void* syscall_stack = PMEM_alloc_phys(core->syscall_stack_size, PMEM_FLAG_IDENTITY_MAPPED);
    core->syscall_stack = (char*)syscall_stack + core->syscall_stack_size;
    
    wrmsr(MSR_KERNEL_GS_BASE, (u64)core);


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

    apic_msr = (apic_msr & ~0xFFFFFC00LLU) | acpi_lapic_address | ((u64)1 << 11); // enable APIC, disable x2APIC, keep reserved and BSP bits (BSP = boot processor)
    wrmsr(IA32_APIC_BASE_MSR, apic_msr);
    // printf("Write new: IA32_APIC_BASE_MSR=%x\n", apic_msr);

    // Important that we set up APIC_SPURIOUS early.
    // APIC Timer or interrupts in general on real hardware (my laptop) won't get setup correctly.
    g_lapic_base[APIC_SPURIOUS/4] = APIC_SOFTWARE_ENABLE | 0xFF;
    // printf("SVR: %x\n", g_lapic_base[APIC_SPURIOUS]);

    int lapic_id = g_lapic_base[APIC_APICID/4] >> 24;
    // printf("apic id: %d\n", lapic_id);

    // g_lapic_base[APIC_SPURIOUS/4] = APIC_SOFTWARE_ENABLE | 0xFF;

    // Reset APIC to known state. (may or may not be necessary but certainly doesn't hurt)
    u32 tmp;
    g_lapic_base[APIC_DFR/4] = 0xFFFFFFFF; // reset Destination Format Register
    tmp = g_lapic_base[APIC_LDR/4];
    tmp &= 0x00FFFFFF;
    tmp |= 1;
    g_lapic_base[APIC_LDR/4] = tmp;
    g_lapic_base[APIC_LVT_TMR/4] = APIC_DISABLE;
    g_lapic_base[APIC_LVT_PERF/4] = APIC_NMI;
    g_lapic_base[APIC_LVT_LINT0/4] = APIC_DISABLE;
    g_lapic_base[APIC_LVT_LINT1/4] = APIC_DISABLE;
    g_lapic_base[APIC_TASKPRIOR/4] = 0;

    g_lapic_base[APIC_EOI/4] = 0; // clear EOI

    g_lapic_base[APIC_TMRDIV/4] = 0x3;
    g_lapic_base[APIC_LVT_TMR/4] = 48 | (1 << 17); // periodic mode
    g_lapic_base[APIC_TMRINITCNT/4] = 100000;

    //  @TODO Calibrate APIC timer with HPET.

    // printf("LVT timer=%x\n", g_lapic_base[APIC_LVT_TMR / 4]);

    // printf("ESR=%x\n", g_lapic_base[APIC_ESR/4]);

    sti();
}

u64 hpet_rdtsc_value;

void calibrate_tsc() {
    cli();

    // A lot of the bits we clear are probably already cleared but we try to be very specific
    // with how we set up the HPET because laptop has been a little quirky.

    const u64 capabilities = *(u64*)(acpi_hpet_address + 0x0);
    volatile u64* configuration  = (u64*)(acpi_hpet_address + 0x10);
    volatile u64* interrupt_status = (u64*)(acpi_hpet_address + 0x20);
    volatile u64* main_counter     = (u64*)(acpi_hpet_address + 0xF0);
    u32 counter_clk_period = capabilities >> 32;
    u32 count_size_cap = (capabilities >> 13) & 1;
    u32 num_tim_cap = (capabilities >> 8) & 0x1F;

    // Set to some arbitrary value in case we can't calibrate.
    // Better than nothing.
    tsc_per_sec = 5000000000; // 5e9


    // @TODO Optimize by doing less repetitive reads and writes to registers.


    volatile u64* chosen_timer = NULL;
    volatile u64* chosen_comp = NULL;

    int irq_number = 2;
    int timer_index = 0;
    while (timer_index < num_tim_cap) {
        volatile u64* hpet_timer = (u64*)(acpi_hpet_address + 0x100 + 0x20*timer_index);
        volatile u64* hpet_comp  = (u64*)(acpi_hpet_address + 0x108 + 0x20*timer_index);
        timer_index++;

        // Clear bits Tn_FSB_EN_CNF, Tn_TYPE_CNF, 32MODE_CNF, Tn_INT_ENB_CNF.
        // In human terms: Disables FSB interrupt mapping, disable 32-bit force mode,
        //   disables periodic mode, and disables interrupt for this timer.
        *hpet_timer = (*hpet_timer & ~0x410CLU);

        // Iterate IRQ numbers till we find a usable one.
        irq_number = 2;
        u32 int_route_cap = *hpet_timer >> 32;
        // printf("Interrupt map %d: %x\n", timer_index-1, int_route_cap);
        while (irq_number < 32) {
            // Check that timer supports the IRQ
            if (((1 << irq_number) & int_route_cap)) {
                // Or the IRQ number and keep other bits untouched
                *hpet_timer = (*hpet_timer & ~0x3E00LU) | (irq_number << 9);

                // Check if we were able to write the value
                u32 written_irq = (*hpet_timer >> 9) & 0x1F;
                if (written_irq == irq_number) {
                    u32 low  = *hpet_timer;
                    u32 high = *hpet_timer >> 32;
                    // printf("Result: %x %x\n", high, low);
                    chosen_timer = hpet_timer;
                    chosen_comp = hpet_comp;
                    break;
                }
                // If note then we did something wrong.
                // We assume timer doesn't support the IRQ number.
            }
            // printf("Failed timer=%d irq=%d\n", timer_index-1, irq_number);
            irq_number++;
        }
        if (irq_number < 32) {
            break;
        }
        // Failed.
        // printf("Failed timer %d\n", timer_index-1);
    }

    if (!chosen_timer) {
        printf("Could not find supported timer\n");
        sti();
        return;
    }
    

    // Enable IRQ number on the IOAPIC. We map to interrupt vector 34.
    // @TODO Should we use edge or level trigger. Are both always available?
    // @TODO We assume APIC ID = 0. We will want to do this per Processor in the future.

    // @TODO Move this complexity elsewhere.
    int calculated_irq_number = 0;
    void* ioapic_address = NULL;
    for (int i=0;i<acpi_ioapic_array_len;i++) {
        IOAPIC_Info* ioapic = &acpi_ioapic_array[i];
        u32 config = cpuReadIoApic((void*)ioapic->address, 0x1);
        int irq_limit = (config>>16) & 0xFF;

        if (irq_number >= ioapic->interruptBaseNumber && irq_number < ioapic->interruptBaseNumber + irq_limit) {
            calculated_irq_number = irq_number - ioapic->interruptBaseNumber;
            ioapic_address = (void*)ioapic->address;
            break;
        }
    }
    if (!ioapic_address) {
        printf("Could not find IRQ (globalSystemInterrupt) in any IOAPIC (ACPI can provide multiple)\n");
        sti();
        return;
    }

    cpuWriteIoApic(ioapic_address, 0x10 + 2 * calculated_irq_number, 34);
    cpuWriteIoApic(ioapic_address, 0x11 + 2 * calculated_irq_number, 0);

    // 100 millisecond second
    u64 delay = (100000000LU*1000000LU)/counter_clk_period;

    // @TODO We need to handle overflow if 32-bit timer/counter is used.
    //   Real hardware uses 32-bit timers, QEMU supports 64-bit timer/counter.
    //   One option is to reset the counter which we can't if a timer is already running.
    //   I have two timers in QEMU and ond the laptop so we will have to be selected how
    //   we use them. Meaning, we know that it's safe to reset here when calibrating tsc.
    //   Since we do this in kernel startup counter will be small anyway.
    //   May recalibrate some time though. For other processors for example.
    // *main_counter = 0;
    
    hpet_rdtsc_value = 0; // Reset previous value (if we call calibrate_tsc again for whatever reason)
    *chosen_timer = *chosen_timer | 4; // Enable interrupts
    *chosen_comp = *main_counter + delay;

    sti();

    u64 start = rdtsc();

    // Keep reserved bits, disable/clear legacy replacment bit and enable main counter.
    *configuration = (*configuration & ~3LU) | 1;

    // Wait for interrupt to set this value
    while (hpet_rdtsc_value == 0) {
        pause();
    }

    cpuWriteIoApic(ioapic_address, 0x10 + 2 * calculated_irq_number, 0x100); // Disable interrupt we are done.
    cpuWriteIoApic(ioapic_address, 0x11 + 2 * calculated_irq_number, 0);

    u64 diff = hpet_rdtsc_value - start;
    tsc_per_sec = diff*10; // We measured 100 milliseconds, multiply by 10 and we get a full second

    printf("HPET measured: %d MHz\n", tsc_per_sec/1000000);

}
void hpet_isr() {
    hpet_rdtsc_value = rdtsc();
    // printf("Triggered %d\n", hpet_rdtsc_value/1000);
    
    if (g_lapic_base)
        g_lapic_base[APIC_EOI/4] = 0; // clear EOI
}


void CPU_reset() {
    acpi_system_reset();
}



void CPU_sleep(u64 nanoseconds) {
    // tsc_per_sec can be assumed to be about 10e9-1e9
    // 1-10 second sleep can cause overflow problems.
    // Below crudely prevents the problem.
    u64 target;
    if (nanoseconds > 700000000) {
        // Do this if sleeping for more than 700 ms
        // Will overflow if sleeping around 1.66 - 16.66 minutes which is
        // a very questionable amount of time to sleep for.
        target = ((nanoseconds/100) * tsc_per_sec)/10000000  + rdtsc();
    } else {
        target = (nanoseconds * tsc_per_sec)/1000000000  + rdtsc();
    }

    while(1) {
        u64 now = rdtsc();
        if (now >= target)
            break;
        pause();
    }
}

int CPU_get_core_index() {
    if (g_lapic_base) {
        return g_lapic_base[APIC_APICID/4] >> 24;
    } else {
        u32 eax, ebx, ecx, edx;
        cpuid(1, 0, &eax, &ebx, &ecx, &edx);
        return ebx >> 24;
    }
}

int CPU_get_core_count() {
    return acpi_lapic_ids_len;
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

void CPU_start_core(u32 apic_id) {
    cli();

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

    sti();
}

_align(4096) u8  initial_ap_stack[CORE_LIMIT * 0x1000]; // 4K stack for each AP. They can setup more later.
_align(4096) u32 initial_ap_stack_top;

// ap = Application Processor, BSP = Bootstrap processor?
void ap_entry(int id) {
    CPU_enable_extensions();
    
    int lapic_id = g_lapic_base[APIC_APICID/4] >> 24;
    printf("AP #%d started (edi=%d)\n", lapic_id, id);
    
    int coreIndex = CPU_get_core_index();
    EXEC_Core* core = &cores[coreIndex];

    core->active_thread = 0;
    core->threads[core->active_thread].used = true;
    // @TODO I think every core needs an idle thread which is only chosen
    //   If there's no other thread to execute.

    init_apic();
    init_syscall();

    while (1) pause();
}



void CPU_enable_extensions() {
    // Move to assembly and kernel_start and AP trampoline?

    // @TODO Implement optional AVX support (nice for memcopies).
    //   Need to properly save it when context switching.
    //   I can't get avx to work in QEMU. Maybe some Windows -> WSL -> KVM -> QEMU issues (understandable).

    u32 eax, ebx, ecx, edx;
    // cpuid(7, 0, &eax, &ebx, &ecx, &edx);
    // bool has_fsgsbase = ebx & (1 << 0);
    // printf("Has fs %d, %d\n", has_fsgsbase, ebx);

    // cpuid(1, 0, &eax, &ebx, &ecx, &edx);
    // if ((ecx & (1 << 27)) == 0) {
    //     printf("NO AVX support!\n");
    //     while (1) pause();
    // } else {
    //     printf("AVX support?\n");
    // }

    // cpuid(1, 0, &eax, &ebx, &ecx, &edx);
    // printf("0x1 0x0 -> eax=%x ebx=%x ecx=%x edx=%x\n", eax, ebx, ecx, edx);
    // cpuid(7, 0, &eax, &ebx, &ecx, &edx);
    // printf("0x7 0x0 -> eax=%x ebx=%x ecx=%x edx=%x\n", eax, ebx, ecx, edx);
    
    cpuid(0x80000001, 0, &eax, &ebx, &ecx, &edx);
    // printf("0x80000001 0x0 -> eax=%x ebx=%x ecx=%x edx=%x\n", eax, ebx, ecx, edx);
    if (0 == (edx & 0x100000)) { // bit 20 NX feature, execute disable
        printf("WAAAH, NX execute disable feature not present!?\n");
    }

    // cpuid(0x80000008, 0, &eax, &ebx, &ecx, &edx);
    // printf("0x80000008 0x0 -> eax=%x ebx=%x ecx=%x edx=%x\n", eax, ebx, ecx, edx);
    
    u64 efer = rdmsr(MSR_IA32_EFER);
    efer |= 1LU<<11; // NXE execute disable bit (for 64-bit intel)
    wrmsr(MSR_IA32_EFER, efer);

    asm (
        "mov %cr0, %rax\n"
        "or $(1 << 1), %rax\n"    // MP
        "and $~(1 << 2), %rax\n"  // EM
        "and $~(1 << 3), %rax\n"  // TS
        "mov %rax, %cr0\n"
        "mov %cr4, %rax\n"
        "or $(1 << 9), %rax\n"  // OSFXSR
        "or $(1 << 10), %rax\n" // OSXMMEXCPT
        // I freeze on these? check cpuid
        // "or $(1 << 16), %rax\n" // FSGSBASE
        // "or $(1 << 18), %rax\n" // OSXSAVE
        "mov %rax, %cr4\n"
    );

    // Enable AVX support.
    // Used by memcpy_fast.
    // asm (
    //     "mov $1, %ecx\n"
    //     "xgetbv\n"
    //     "or $0x6, %eax\n" // Set XCR0.SSE, XCR0.AVX
    //     "xsetbv\n"
    // );
}