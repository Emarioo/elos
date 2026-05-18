/*
    These synchronization primitives assumes no re-entry
    and in-parameter is 4-byte aligned.
*/

.intel_syntax noprefix

.section .text


.global LOCK
.spin:
    pause
    test word ptr [rdi], 1
    jnz .spin
LOCK:
    lock bts word ptr [rdi], 0
    jc .spin
    ret

.global UNLOCK
UNLOCK:
    mov word ptr [rdi], 0
    ret

# Usable for interrupt lock as well
.global IS_LOCKED
IS_LOCKED:
    cmp word ptr [rdi], 0
    setz al
    ret



.global LOCK_INT
LOCK_INT:
    pushfq
    pop rax
    cli

.try_again:
    lock bts word ptr [rdi], 0
    jc .spin_enter
    mov word ptr [rdi+2], ax
    ret

.spin_enter:
    bt ax, 9
    jnc .spin_int
    sti
.spin_int:
    pause
    test word ptr [rdi], 1
    jnz .spin_int
    cli
    jmp .try_again

.global UNLOCK_INT
UNLOCK_INT:
    mov ax, word ptr [rdi+2]
    mov dword ptr [rdi], 0
    bt ax, 9
    jnc .end
    sti
.end:
    ret
