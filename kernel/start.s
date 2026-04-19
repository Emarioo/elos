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
    push rbp # push to align stack to 16 bytes

    # Save BootAPI to non-volatile register
    mov rbx, rcx
    
    call zero_bss

    // mov rsp, __stack_end
    mov rdi, rbx
    call kernel_entry


zero_bss:
    
    lea rsi, __bss_start
    lea rdi, __bss_start + 16

    xor rax, rax

.zero_loop:
    cmp rsi, rdi
    je .end

    mov [rsi], rax
    add rsi, 8

    cmp rsi, rdi
    jne .zero_loop
.end:

    ret

.section .note.GNU-stack,"",@progbits
