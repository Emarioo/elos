KERNEL_ADDR EQU 0x8000

[bits 16]
[org KERNEL_ADDR]

main:
    mov si, num
    ; mov si, hello
    call puts

    cli
    hlt

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

.halt:
    jmp .halt

hello: db 'Hello world!', 13, 10, 0, 'Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!',0

num: db 'Bigger section loaded',13,10,0
    
