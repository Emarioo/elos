.intel_syntax noprefix

.section .text

// void memcpy(void* dst, const void* src, int size) {
.global memcpy
memcpy:
    mov     rax, rdi

.set INCREMENT,8

.loop32:
    cmp     rdx, INCREMENT
    jb      .tail
    mov r8, rdi
    mov rcx, [rsi]
    mov [rdi], rcx

    // @TODO AVX support. Make a memcpy_fast version?
    //   Need to get AVX working in qemu so i can test it first?
    // vmovdqu ymm0, [rsi]
    // vmovdqu [rdi], ymm0

    add     rsi, INCREMENT
    add     rdi, INCREMENT
    sub     rdx, INCREMENT
    jmp     .loop32

.tail:
    test    rdx, rdx
    jz      .done

.tail_loop:
    mov     cl, [rsi]
    mov     [rdi], cl

    inc     rsi
    inc     rdi
    dec     rdx
    jnz     .tail_loop

.done:
    // vzeroupper
    mov rax, r8
    ret
