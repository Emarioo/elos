[org 0x7c00]

hello:
    db "Hello World!", 0

mov bx, hello
call print_str

jmp END

; prints string in bx, remember null termination 
print_str:
    mov ah, 0x0e
print_str_loop:
    cmp [bx], 0
    je print_str_end
    mov al, [bx]
    int 0x10
    inc bx
    jmp print_str_loop
print_str_end:
    ret

END:
    jmp $
    times 510-($-$$) db 0
    db 0x55, 0xaa