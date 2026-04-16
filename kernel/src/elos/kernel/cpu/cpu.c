
#include "elos/cpu.h"



void init_gdt_idt();


void CPU_init(BootAPI* boot_api) {

    init_gdt_idt();
}


// TODO: GDT,IDT tables should be placed elsewhere
#pragma pack(push, 1)
typedef struct GDT_IDT_Register {
    uint16_t size;
    uint64_t addr;
} GDT_IDT_Register;
#pragma pack(pop)

static GDT_IDT_Register _gdt_register;
static GDT_IDT_Register _idt_register;

static uint64_t _gdt[3];
static uint64_t _idt[1024];

void init_gdt_idt() {
    
    _gdt[0] = 0;
    _gdt[1] = (( 0b0010LLU ) << 52) | (( 0b10011010LLU ) << 40);
    _gdt[2] = (( 0b0000LLU ) << 52) | (( 0b10010010LLU ) << 40);

    // @TODO: Setup ring-3 user task system segments
    
    _gdt_register.size = sizeof(_gdt);
    _gdt_register.addr = (uint64_t)&_gdt;
    
    // @TODO: Interrupt descriptor table

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
}
