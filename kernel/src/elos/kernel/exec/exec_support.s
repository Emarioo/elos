.intel_syntax noprefix

.extern thread_bootstrap
.extern EXEC_timer_handler
.extern EXEC_syscall_handler

.extern g_lapic_base

.set MSR_FS_BASE, 0xC0000100
.set MSR_GS_BASE, 0xC0000101

.set APIC_EOI, 0x0B0

.macro SAVE_CONTEXT
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

    push 0 # push reserved to align struct
.endm


.macro RESTORE_CONTEXT
    pop rax # pop reserved to align struct

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

    mov ax, 0x0
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    pop rax
    pop rbp
.endm

    .global timer_isr
timer_isr:
    # interrupt pushed
    #   rip
    #   cs
    #   rflags
    #   rsp
    #   ss
    # If 32-bit compatibility process is interrupted then 64-bit registers are pushed since we ENTER 64-bit long mode.

    SAVE_CONTEXT

    mov rax, g_kernelPageTable
    mov cr3, rax

    mov rdi, rsp

    call EXEC_timer_handler
    
    // @TODO Every 1 ms we want to schedule or execute something.
    //   We are not making real-time OS but we may want to use periodic
    //   timer interrupts using processor feature or schedule timer interrupt
    //   for one-shot mode (TSC deadline) as soon as we get the interrupt.
    mov rdi, [g_timer_frequency_ns]
    call CPU_schedule_timer_interrupt

    # The called function will memcpy the new process's registers into the stack (rdi) we gave it.
    
    # Clear EOI (end of interrupt), so next timer interrupt can arrive (and other interrupts like keyboard)
    mov rbx, g_lapic_base
    mov DWORD PTR [rbx + APIC_EOI], 0

    RESTORE_CONTEXT    

    iretq

.global timer_isr_ret32
timer_isr_ret32:
    
    // @TODO Every 1 ms we want to schedule or execute something.
    //   We are not making real-time OS but we may want to use periodic
    //   timer interrupts using processor feature or schedule timer interrupt
    //   for one-shot mode (TSC deadline) as soon as we get the interrupt.
    mov rdi, [g_timer_frequency_ns]
    call CPU_schedule_timer_interrupt

    # The called function will memcpy the new process's registers into the stack (rdi) we gave it.
    
    # Clear EOI (end of interrupt), so next timer interrupt can arrive (and other interrupts like keyboard)
    mov rbx, g_lapic_base
    mov DWORD PTR [rbx + APIC_EOI], 0
    
    RESTORE_CONTEXT

    // @TODO Optimize this. Can't put above RESTORE_CONTEXT because we need to compute SS offset in the ContextFrame struct.
    push rax
    mov rax, [rsp + (1 + 4) * 8] # extract SS from interrupt frame
    mov ds, ax
    pop rax

    // When returning to 32-bit compatibility mode we use iret with 32-bit operand size.
    // We also make sure registers (esp,eip,cs,ss,eflags) are 32-bit on the stack (handled in EXEC_timer_handler).
    iretd


.set SYSCALL_STACK_OFFSET, 0
.set USER_STACK_OFFSET, 8
.set RESCHEDULE_OFFSET, 16

.global syscall_handler
syscall_handler:

    swapgs

    mov gs:USER_STACK_OFFSET, rsp
    mov rsp, gs:SYSCALL_STACK_OFFSET

    push r11 # User RFLAGS
    push rcx # User RIP

    mov rcx, r10

    call EXEC_syscall_handler
    
    pop rcx
    pop r11
    mov dil, gs:RESCHEDULE_OFFSET

    // @TODO @SECURITY Before returning we must reset touched registers because
    //   they may contain kernel information.

    cmp dil, 0
    jnz syscall_reschedule

    mov rsp, gs:USER_STACK_OFFSET
    swapgs

    # @TODO @SECURITY Do not allow RCX (user RIP) to be above 48-bit address space. crashes, security problems?
    #   We can simply not allow anyone to map anything above that address and we should be fine. 256 TB of addressable space should be enough anyway.

    sti
    /* @TODO @SECURITY
        "For both AMD and Intel, it is up to the kernel to switch stack back to the userspace stack before executing SYSRET. This opens a race condition where the NMIs and MCEs exception handlers will be executed on a guest controlled stack. For 64bit mode, the kernel must use Interrupt Stack Tables to safely move NMIs/MCEs onto a properly designated kernel stack. For 32bit mode AMD systems, the kernel must use Task Gates for NMIs and MCEs to switch stack." - osdev.org
    */
    sysretq

syscall_reschedule:

    # interrupt would have pushed
    #   rip
    #   cs
    #   rflags
    #   rsp
    #   ss

    mov r8, gs:USER_STACK_OFFSET
    swapgs

    # Kernel is not allowed to make a syscall.
    # It's therefore fine to restore ring-3 privileges.
    push 0x23  # ss   USER_DATA_SEGMENT
    push r8    # rsp
    push r11   # rflags
    push 0x2B  # cs   USER_CODE_SEGMENT
    push rcx   # rip

    SAVE_CONTEXT

    mov rax, g_kernelPageTable
    mov cr3, rax

    mov rdi, rsp

    call EXEC_timer_handler

    RESTORE_CONTEXT

    iretq


.global syscall_handler32
syscall_handler32:
    /*
        These were prepared by user process
            eax - syscall id
            ebx - arg 0
            ecx -
            edx -
            esi -
            edi -
            ebp - arg 5

        Software interrupt would have pushed
            rip
            cs
            rflags
            rsp
            ss

        We switched to task segment stack on the interrupt.
    */

    // Save non-volatile registers
    push rbx
    push rsi
    push rdi
    push rbp

    // Save to temp register since ecx <-> esi need to swap places.
    mov r10d, esi

    // Convert from 32-bit syscall ABI to 64-bit syscall ABI
    mov edi, ebx
    mov esi, ecx
    // mov edx, edx    no need to move to same register
    mov ecx, r10d
    mov r8d, edi
    mov r9d, ebp
    

    call EXEC_syscall_handler

    // @TODO @SECURITY Before returning we must reset touched registers because
    //   they may contain kernel information.

    swapgs
    mov dil, gs:RESCHEDULE_OFFSET
    swapgs

    cmp dil, 0
    jnz syscall_reschedule32

    // Restore non-volatile registers
    pop rbp
    pop rdi
    pop rsi
    pop rbx

    mov rcx, [rsp + 4 * 8] # extract SS from interrupt frame
    mov ds, cx

    // Convert 64-bit registers (rip,cs,rflags,rsp,ss) to 32-bit registers (eip,cs,eflags,esp,ss)
    mov rcx, [rsp + 0]
    mov [rsp + 0], ecx
    mov rcx, [rsp + 8]
    mov [rsp + 4], ecx
    mov rcx, [rsp + 16]
    mov [rsp + 8], ecx
    mov rcx, [rsp + 24]
    mov [rsp + 12], ecx
    mov rcx, [rsp + 32]
    mov [rsp + 16], ecx

    iretd

syscall_reschedule32:

    // Restore non-volatile registers
    pop rbp
    pop rdi
    pop rsi
    pop rbx

    # interrupt would have pushed
    #   rip
    #   cs
    #   rflags
    #   rsp
    #   ss

    SAVE_CONTEXT

    mov rax, g_kernelPageTable
    mov cr3, rax

    mov rdi, rsp

    call EXEC_timer_handler
    // We will hop to timer_isr_ret32 if we restore 32-bit compatibility context.
    // It's therefore fine to use iretq.

    RESTORE_CONTEXT

    iretq



.global kernel_thread_reschedule
kernel_thread_reschedule:
    push rbp # Ensure 16-byte alignment

    # interrupt would have pushed
    #   rip
    #   cs
    #   rflags
    #   rsp
    #   ss

    lea r8,  [rsp + 16]   # Stack address minus the return address
    mov rcx, [rsp + 8]    # Return address (rip)

    push 0x10  # ss  KERNEL_DATA_SEGMENT
    push r8    # rsp
    pushfq
    push 0x8  # cs   KERNEL_CODE_SEGMENT
    push rcx   # rip

    SAVE_CONTEXT

    mov rdi, rsp

    call EXEC_timer_handler

    RESTORE_CONTEXT

    iretq
