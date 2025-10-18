KERNEL_ADDR EQU 0x8000

[bits 16]
[org KERNEL_ADDR]

CODE_SEG equ code_descriptor - GDT_start
CODE16_SEG equ code16_descriptor - GDT_start
DATA_SEG equ data_descriptor - GDT_start

VIDEO_MEMORY equ 0xb8000

setup:
    [bits 16]

    mov ax, 0x0 ; delete
    mov ds, ax
    mov es, ax
    mov ss, ax

    ; print something
    mov si, msg_enter_setup
    call puts16

    ; switch to protected mode
    cli                   ; disable interupts
    ; enable A20 does not work at the moment
    ; call A20_enable       ; enable A20 (no wrap on 1MB)
    lgdt [GDT_descriptor] ; load gdt

    mov eax, cr0    ; set protection bit
    or al, 1
    mov cr0, eax
    
    ; Align stack to 4 bytes
    add sp, 3
    and sp, 0xFFFC

    ; jump to protected section
    jmp dword CODE_SEG:pmode

    cli
    hlt
    jmp halt

pmode:
    [bits 32]
    ; A reminder, DO NOT call assembly written for 16-bit mode
    
    ; setup segments?
    mov ax, 0x10
    mov ds, ax
    mov ss, ax

    mov ah, 0x2
    mov al, 'H'
    mov [VIDEO_MEMORY], ax
    mov al, 'i'
    mov [VIDEO_MEMORY+2], ax
    mov al, ' '
    mov [VIDEO_MEMORY+4], ax

    ; cli ; interrupt already cleared
    ; jmp CODE16_SEG:pmode16

    hlt

pmode16:
    [bits 16]

    ; turn of protection bit
    mov eax, cr0
    and al, ~1
    mov cr0, eax

    jmp word 00h:rmode

    hlt

rmode:
    mov ax, 0
    mov ds, ax
    mov ss, ax

    sti

    mov si, msg_hello
    call puts16

halt:
    cli
    hlt
    jmp halt

A20_enable:
    bits 16

    ; TODO: Check if already enabled

    ; disable keyboard
    call A20_wait_input
    mov al, KEYBOARD_DISABLE
    out KEYBOARD_COMMAND_PORT, al

    ; read control output port and flush output buffer
    call A20_wait_input
    mov al, KEYBOARD_READ_OUTPUT_PORT
    out KEYBOARD_COMMAND_PORT, al

    call A20_wait_output
    in al, KEYBOARD_DATA_PORT
    push ax


    call A20_wait_input
    mov al, KEYBOARD_WRITE_OUTPUT_PORT
    out KEYBOARD_COMMAND_PORT, al

    ; turn on A20 bit
    call A20_wait_input
    pop ax
    or al, 2
    out KEYBOARD_DATA_PORT, al

    ; enable keyboard
    call A20_wait_input
    mov al, KEYBOARD_ENABLE
    out KEYBOARD_COMMAND_PORT, al

A20_wait_input:
    in al, KEYBOARD_COMMAND_PORT
    test al, 2
    jnz A20_wait_input
    ret
A20_wait_output:
    in al, KEYBOARD_COMMAND_PORT
    test al, 1
    jnz A20_wait_output
    ret

KEYBOARD_DATA_PORT          equ 0x60
KEYBOARD_COMMAND_PORT       equ 0x64
KEYBOARD_DISABLE            equ 0xAD
KEYBOARD_ENABLE             equ 0xAE
KEYBOARD_READ_OUTPUT_PORT   equ 0xD0
KEYBOARD_WRITE_OUTPUT_PORT  equ 0xD1


puts:
    [bits 32]
    push esi
    push eax
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

puts16:
    [bits 16]
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

; https://en.wikipedia.org/wiki/Segment_descriptor
GDT_start:
    null_descriptor:
        dq 0
    code_descriptor:
        dw 0xffff ; limit
        dw 0 ; base address (0-16)
        db 0 ; base address (16-23)
        db 0b10011010 ; present, ring 0, code segment, executable, direction 0, readable
        db 0b11001111 ; granularity (4k pages, 32-bit protected mode), limit (16-19)
        db 0 ; base address (24-31)
    data_descriptor:
        dw 0xffff ; limit
        dw 0 ; base address (0-16)
        db 0 ; base address (16-23)
        db 0b10010010 ; present, ring 0, data segment, executable, direction 0, readable
        db 0b11001111 ; granularity (4k pages, 32-bit protected mode), limit (16-19)
        db 0 ; base address  (24-31)
    code16_descriptor:
        dw 0xffff ; limit
        dw 0 ; base address (0-16)
        db 0 ; base address (16-23)
        db 0b10011010 ; present, ring 0, code segment, executable, direction 0, readable
        db 0b00001111 ; granularity (1b pages, 16-bit real mode?), limit (16-19)
        db 0 ; base address (24-31)
    data16_descriptor:
        dw 0xffff ; limit
        dw 0 ; base address (0-16)
        db 0 ; base address (16-23)
        db 0b10010010 ; present, ring 0, data segment, executable, direction 0, readable
        db 0b00001111 ; granularity (1b pages, 16-bit real mode?), limit (16-19)
        db 0 ; base address (24-31)
GDT_end:

GDT_descriptor:
    dw GDT_end - GDT_start -1
    dd GDT_start

msg_hello: db 'Hello world!', 13, 10, 0
;  'Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!',0

msg_enter_setup: db 'Enter setup',13, 10, 0
    
