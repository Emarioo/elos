/*
    This assembly script does a few things:
    - Transition from Microsoft x64 Calling convention to System V ABI calling convention
    - Zero .bss section
*/

.intel_syntax noprefix

.section .text

.global _start
_start: # void _start(BootAPI*)
    # rcx = pointer to BootAPI
    push rbp # push to align stack to 16 bytes, not needed since we replace stack below BUT in case we decide not to do that
             # anymore i'm keeping this line here so we don't forget to ensure 16-byte alignment.

    lea rsp, __stack_end

    # Save BootAPI to non-volatile register
    mov rbx, rcx
    
    call zero_bss

    // mov rsp, __stack_end
    mov rdi, rbx
    call kernel_entry


zero_bss:
    
    lea rsi, __bss_start
    lea rdi, __bss_end

    xor rax, rax

.zero_loop:
    cmp rsi, rdi
    jge .end

    mov [rsi], rax
    add rsi, 8

    cmp rsi, rdi
    jne .zero_loop
.end:

    ret

.section .note.GNU-stack,"",@progbits
