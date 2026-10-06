/*
    This assembly script does a few things:
    - Transition from Microsoft x64 Calling convention to System V ABI calling convention
    - Zero .bss section
*/

.intel_syntax noprefix

.section .low_boot,"ax"

.align 4096

.global _start
_start: # void _start(BootAPI*)
    # rcx = pointer to BootAPI
    push rbp # push to align stack to 16 bytes, not needed since we replace stack below BUT in case we decide not to do that
             # anymore i'm keeping this line here so we don't forget to ensure 16-byte alignment.

    lea rax, [pml4]
    mov cr3, rax

    lea rsp, [__stack_end]

    # Save BootAPI to non-volatile register
    # EFI app called this function which uses windows calling convention so we use rcx not rdi
    mov rbx, rcx
    
    call zero_bss

    mov rdi, rbx
    
    lea rax, [kernel_entry]
    call rax


zero_bss:
    
    lea rsi, [__bss_start]
    lea rdi, [__bss_end]

    xor rax, rax

.zero_loop:
    cmp rsi, rdi
    jae .end

    mov [rsi], rax
    add rsi, 8

    cmp rsi, rdi
    jne .zero_loop
.end:

    ret


.align 4096
pml4:
    .quad pdpt_low + 0x003
    .fill 510, 8, 0
    .quad pdpt_high + 0x003

.align 4096
pdpt_low:
    .quad 0x00000000 + 0x083
    .quad 0x40000000 + 0x083
    .quad 0x80000000 + 0x083
    .quad 0xC0000000 + 0x083
    .fill 508, 8, 0
    
.align 4096
pdpt_high:
    .fill 508, 8, 0
    .quad 0x80000000 + 0x083
    .quad 0xC0000000 + 0x083
    .quad 0x00000000 + 0x083
    .quad 0x40000000 + 0x083


.section .note.GNU-stack,"",@progbits
