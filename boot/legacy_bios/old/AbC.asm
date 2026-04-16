mov ah, 0x0e
mov al, 'A'
loop:
    int 0x10
    add al, 'a'-'A'+1

    int 0x10
    add al, 'A'-'a'+1

    cmp al, 'Z'+1
    jne loop

jmp $

times 510-($-$$) db 0
db 0x55, 0xaa