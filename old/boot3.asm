[org 0x7c00]
[bits 16]

start:
    jmp main

puts:
    push si
    push ax
.loop:
    lodsb
    or al, al
    jz .done

    mov ah, 0eh
    int 0x10

    jmp .loop
.done:
    pop ax
    pop si
    ret

main:
    mov ax, 0
    mov ds, ax
    mov es, ax

    mov ss, ax
    mov sp, 0x7C00

    mov si, hello
    call puts

    hlt

.halt:
    jmp .halt

hello: db 'Hello world!', 13, 10, 0

times 510-($-$$) db 0
db 0x55, 0xaa
