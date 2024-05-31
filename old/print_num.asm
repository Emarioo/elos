[org 0x7c00]

mov bp, 0
mov sp, bp

mov ax, 12392
call print_num

jmp END

; prints value in ax
print_num:
    mov bx, 0 ; char count
unravel:
    mov cx, 10
    mov dx, 0 ; needs to be 0 for div to work, why?
    div cx ; ax/10 -> ax will have ax/10, bx has remainder
    
    inc bx
    add dx, '0'
    push dx

    cmp ax, 0
    jne unravel


print_chars:
    pop ax
    mov ah, 0x0e
    int 0x10
    dec bx
    cmp bx, 0
    jne print_chars

    ret

END:
    jmp $
    times 510-($-$$) db 0
    db 0x55, 0xaa