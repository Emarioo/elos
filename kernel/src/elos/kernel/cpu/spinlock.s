/*
    These synchronization primitives assumes no re-entry
    and in-parameter is 4-byte aligned.
*/

.intel_syntax noprefix

.section .text

// IMPLEMENTATION HAS NOT BEEN TESTED.

.global LOCK
.spin:
    pause
    test dword ptr [rdi], 1
    jnz .spin
LOCK:
    lock bts dword ptr [rdi], 0
    jc .spin
    ret

.global UNLOCK
UNLOCK:
    mov dword ptr [rdi], 0
    ret

# Usable for interrupt lock as well
.global IS_LOCKED
IS_LOCKED:
    cmp dword ptr [rdi], 0
    setz al
    ret


// IMPLEMENTATION HAS NOT BEEN TESTED.

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
    test eax, 0x200
    jz .spin_int
    sti
.spin_int:
    pause
    test byte ptr [rdi], 1
    jnz .spin_int
    cli
    jmp .try_again

.global UNLOCK_INT
UNLOCK_INT:
    mov ax, word ptr [rdi+2]
    mov dword ptr [rdi], 0
    cmp ax, 0x200
    jz .end
    sti
.end:
    ret
