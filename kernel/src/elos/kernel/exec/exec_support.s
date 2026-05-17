.intel_syntax noprefix

.extern thread_bootstrap
.extern EXEC_interrupt

.extern g_lapic_base



.set APIC_EOI, 0x0B0


    .global timer_isr
timer_isr:
    cli
    # Every time this is triggered we want to round robin threads.
    
    # First we must store all CPU state for the thread.

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

    mov rdi, rsp

    # @TODO Save float registers
    call EXEC_interrupt

    # Switch to different thread, the stack of that thread has all registers pushed
    mov rsp, rax
    
    # Clear EOI (end of interrupt), so next timer interrupt can arrive (and other interrupts like keyboard)
    mov rbx, g_lapic_base
    mov DWORD PTR [rbx + APIC_EOI], 0
    
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

    sti
    iretq


.global EXEC_terminate_self_end
.global EXEC_terminate_self
EXEC_terminate_self:
    int 48
EXEC_terminate_self_end:
    jmp EXEC_terminate_self_end
