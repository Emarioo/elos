/*
    This assembly script does a few things:
    - Transition from Microsoft x64 Calling convention to System V ABI calling convention
    - Zero .bss section
*/

.intel_syntax noprefix

.section .text

.global _start
_start:
    # rcx = pointer to BootAPI

    # Save BootAPI to non-volatile register
    mov rdx, rcx
    
    call zero_bss

    mov rdi, rbx
    call kernel_entry


zero_bss:
    
    mov rsi, __bss_start
    mov rdi, __bss_end

    xor rax, rax

.zero_loop:
    cmp rsi, rdi

    mov [rsi], rax
    add rsi, 8

    jne .zero_loop

    ret

.section .note.GNU-stack,"",@progbits
