.intel_syntax noprefix

.extern thread_bootstrap
.extern EXEC_interrupt

.extern g_lapic_base



.set APIC_EOI, 0x0B0

    .global timer_isr
timer_isr:
    # interrupt pushed
    #   rip
    #   cs
    #   rflags
    #   rsp
    #   ss

    push rbp
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    sub rsp, 128

    movsd [rsp],         xmm0
    movsd [rsp +  1*8],  xmm1
    movsd [rsp +  2*8],  xmm2
    movsd [rsp +  3*8],  xmm3
    movsd [rsp +  4*8],  xmm4
    movsd [rsp +  5*8],  xmm5
    movsd [rsp +  6*8],  xmm6
    movsd [rsp +  7*8],  xmm7
    movsd [rsp +  8*8],  xmm8
    movsd [rsp +  9*8],  xmm9
    movsd [rsp + 10*8], xmm10
    movsd [rsp + 11*8], xmm11
    movsd [rsp + 12*8], xmm12
    movsd [rsp + 13*8], xmm13
    movsd [rsp + 14*8], xmm14
    movsd [rsp + 15*8], xmm15

    mov rdi, rsp

    call EXEC_interrupt

    # Switch to different thread, the stack of that thread has all registers pushed
    mov rsp, rax
    
    # Clear EOI (end of interrupt), so next timer interrupt can arrive (and other interrupts like keyboard)
    mov rbx, g_lapic_base
    mov DWORD PTR [rbx + APIC_EOI], 0
    
    movsd xmm0,  [rsp]
    movsd xmm1,  [rsp +  1*8]
    movsd xmm2,  [rsp +  2*8]
    movsd xmm3,  [rsp +  3*8]
    movsd xmm4,  [rsp +  4*8]
    movsd xmm5,  [rsp +  5*8]
    movsd xmm6,  [rsp +  6*8]
    movsd xmm7,  [rsp +  7*8]
    movsd xmm8,  [rsp +  8*8]
    movsd xmm9,  [rsp +  9*8]
    movsd xmm10, [rsp + 10*8]
    movsd xmm11, [rsp + 11*8]
    movsd xmm12, [rsp + 12*8]
    movsd xmm13, [rsp + 13*8]
    movsd xmm14, [rsp + 14*8]
    movsd xmm15, [rsp + 15*8]
    add rsp, 128

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    pop rbp

    iretq


.global EXEC_terminate_self_end
.global EXEC_terminate_self
EXEC_terminate_self:
    int 48
EXEC_terminate_self_end:
    jmp EXEC_terminate_self_end
