# Code is included in the Kernel Image but later one page of the code is copied
# to some address in 16-bit address space. The code cannot be larger than one page unless
# we update code that does the copying.

# BSP = Bootstrap Processor
# AP  = Application Processor

.intel_syntax noprefix

    .section .text

.set IA32_EFER, 0xC0000080

    .global ap_trampoline
    .code16
ap_trampoline:
    # We start in 16-bit mode.

    cli
    cld
    ljmp 0x00:0x8040

    .align 16
_8010_GDT_table:
    .long 0, 0
    .long 0x0000FFFF, 0x00CF9A00 # flat code
    .long 0x0000FFFF, 0x008F9200 # flat data
    .long 0x00000068, 0x00CF8900 # tss

_8030_GDT_value:
    .word _8030_GDT_value - _8010_GDT_table - 1
    .long 0x8010
    .long 0, 0

    .align 64
_8040:
    xor ax, ax
    mov ds, ax

    lgdt [0x8030]
    mov eax, cr0
    or  eax, 1
    mov cr0, eax
    jmp 0x08:0x8060
    
    .align 32
    .code32
_8060:
    mov ax, 16
    mov ds, ax
    mov ss, ax

    # Enable PAE (physical address extension) and PGE
    mov eax, cr4
    or eax, 0xA0
    mov cr4, eax

    # Load root page table
    # (same as the Bootstrap Processor for now)
    mov eax, [rootTable]
    mov cr3, eax

    # Enable long mode in EFER
    mov ecx, IA32_EFER
    rdmsr
    or eax, 0x100
    wrmsr

    # Enable paging
    mov eax, cr0
    or eax, 0x80000000 # PG bit (paging)
    mov cr0, eax

    lgdt [_gdt_register] # Prepared by BSP in Kernel C code
    lidt [_idt_register]

    // Jump to long mode
    ljmp 0x08:0x8100

    .align 256
    .code64
_8100:
    // 64-bit long mode with paging
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax

    # Get Local APIC ID
    mov eax, 1
    cpuid
    shr ebx, 24
    mov edi, ebx

    # Get small temporary stack for this processor
    lea rsp, [initial_ap_stack_top]
    shl ebx, 12
    sub rsp, rbx
    
    lea rbx, [ap_entry]
    call rbx
    #  Should not return.
