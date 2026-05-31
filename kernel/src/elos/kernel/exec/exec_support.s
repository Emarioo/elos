.intel_syntax noprefix

.extern thread_bootstrap
.extern EXEC_timer_handler
.extern EXEC_syscall_handler

.extern g_lapic_base

.set MSR_FS_BASE, 0xC0000100
.set MSR_GS_BASE, 0xC0000101

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

    mov rax, cr3
    push rax
    
    mov ecx, MSR_GS_BASE
    rdmsr
    shl rdx, 32
    or rax, rdx
    push rax
    
    mov ecx, MSR_FS_BASE
    rdmsr
    shl rdx, 32
    or rax, rdx
    push rax
    
    mov rax, g_kernelPageTable
    mov cr3, rax

    mov rdi, rsp

    call EXEC_timer_handler

    # The called function will memcpy the new process's registers into the stack (rdi) we gave it.
    
    # Clear EOI (end of interrupt), so next timer interrupt can arrive (and other interrupts like keyboard)
    mov rbx, g_lapic_base
    mov DWORD PTR [rbx + APIC_EOI], 0
    
    pop rax
    mov rdx, rax
    shr rdx, 32
    mov eax, eax
    mov ecx, MSR_FS_BASE
    wrmsr
    
    pop rax
    mov rdx, rax
    shr rdx, 32
    mov eax, eax
    mov ecx, MSR_GS_BASE
    wrmsr

    pop rax
    mov cr3, rax

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

    mov rax, [rsp + 6 * 8] # extract SS from interrupt frame
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    pop rax
    pop rbp

    iretq


.set SYSCALL_STACK_OFFSET, 0
.set USER_STACK_OFFSET, 8

.global syscall_handler
syscall_handler:

    swapgs

    mov gs:USER_STACK_OFFSET, rsp
    mov rsp, gs:SYSCALL_STACK_OFFSET

    push r11
    push rcx

    mov rcx, r10

    call EXEC_syscall_handler

    pop rcx
    pop r11

    mov rsp, gs:USER_STACK_OFFSET

    swapgs

    // @TODO @SECURITY Do not allow RCX (user RIP) to be above 48-bit address space. crashes, security problems?
    //   We can simply not allow anyone to map anything above that address and we should be fine. 256 TB of addressable space should be enough anyway.

    sti
    /* @TODO @SECURITY
        "For both AMD and Intel, it is up to the kernel to switch stack back to the userspace stack before executing SYSRET. This opens a race condition where the NMIs and MCEs exception handlers will be executed on a guest controlled stack. For 64bit mode, the kernel must use Interrupt Stack Tables to safely move NMIs/MCEs onto a properly designated kernel stack. For 32bit mode AMD systems, the kernel must use Task Gates for NMIs and MCEs to switch stack." - osdev.org
    */
    sysretq



.global EXEC_terminate_self_end
.global EXEC_terminate_self
EXEC_terminate_self:
    int 48
EXEC_terminate_self_end:
    jmp EXEC_terminate_self_end
