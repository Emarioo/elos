[org 0x7c00]

; mov ah, 0x0e

buffer:
    times 10 db 0
    mov bx, buffer
    
    mov [bx], al
    inc bx

loops:
    mov ah, 0 ; wait for key
    int 0x16 ; some keyboard interrupt?

    cmp ah, 28 ; scancode for enter key
    ; cmp al, 0x1c ; works too (character for enter)
    je end

    mov ah, 0x0e
    int 0x10

    mov [bx], al
    inc bx

    cmp bx, buffer+11
    jne loops

    jmp skip1
end:
    mov ah, 0x0e
    mov al, 10
    int 0x10
skip1:


mov ah, 0x0e
; mov al, '\n'
mov bx, buffer

loop2:
    mov al, [bx]
    int 0x10
    inc bx

    cmp al, 0
    je end2

    cmp bx, buffer+11
    jne loop2

end2:

jmp $

times 510-($-$$) db 0
db 0x55, 0xaa