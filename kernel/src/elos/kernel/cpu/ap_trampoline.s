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
    mov eax, [0x8200]
    mov eax, [eax]
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

    # Get Local APIC ID
    mov eax, 1
    cpuid
    shr ebx, 24
    mov edi, ebx

    imul ebx, 10

    mov eax, [0x8204]
    lgdt [eax + ebx] # Prepared by BSP in Kernel C code
    mov eax, [0x8208]
    lidt [eax]

    // Jump to long mode
    ljmp 0x08:0x8100 # KERNEL_CODE_SEGMENT

    .align 256
    .code64
_8100:
    # 64-bit long mode with paging
    mov ax, 0x10 # KERNEL_DATA_SEGMENT
    mov ds, ax
    mov es, ax
    mov ss, ax
 
    # Load Task State Segment
    mov ax, 0x38 # TASK_STATE_SEGMENT
    ltr ax

    # Get small temporary stack for this processor
    mov ebx, edi # ebx = apic id
    movabs rax, initial_ap_stack_top
    mov rsp, rax
    shl ebx, 12
    sub rsp, rbx
    
    movabs rax, ap_entry
    mov rbx, rax
    call rbx
    #  Should not return.
trampoline_spin:
    jmp trampoline_spin

    .align 256
    .code32
_8200:

rootPage_phys_address: .long 0
gdt_phys_address: .long 0
idt_phys_address: .long 0
