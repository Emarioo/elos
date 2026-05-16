/*
    These synchronization primitives assumes no re-entry
    and in-parameter is 4-byte aligned.
*/

.intel_syntax noprefix

.section .text


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

.global IS_LOCKED
IS_LOCKED:
    cmp dword ptr [rdi], 0
    setz al
    ret
