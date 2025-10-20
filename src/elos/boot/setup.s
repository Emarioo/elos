.intel_syntax noprefix

.set KERNEL_ADDR, 0x8000

# specified by linker script
.section .setup, "ax"

.set CODE_SEG, code_descriptor - GDT_start
.set CODE16_SEG, code16_descriptor - GDT_start
.set DATA_SEG, data_descriptor - GDT_start

.set VIDEO_MEMORY, 0xb8000

.code16
setup:
    mov ax, 0x0 # delete
    mov ds, ax
    mov es, ax
    mov ss, ax

    # print something
    mov si, msg_enter_setup
    call puts16

    # switch to protected mode
    cli                   # disable interupts
    # enable A20 does not work at the moment
    # call A20_enable       # enable A20 (no wrap on 1MB)
    lgdt [GDT_descriptor] # load gdt

    mov eax, cr0    # set protection bit
    or al, 1
    mov cr0, eax
    
    # Align stack to 4 bytes
    add sp, 3
    and sp, 0xFFFC

    # jump to protected section
    jmp far ptr CODE_SEG:pmode

    cli
    hlt
    jmp halt

.code32
pmode:
    # A reminder, DO NOT call assembly written for 16-bit mode
    
    # setup segments?
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

    # cli # interrupt already cleared
    # jmp CODE16_SEG:pmode16

    hlt

.code16
pmode16:
    # turn of protection bit
    mov eax, cr0
    and al, ~1
    mov cr0, eax

    jmp far ptr 0x00:rmode

    hlt

.code16
rmode:
    mov ax, 0
    mov ds, ax
    mov ss, ax

    sti

    mov si, msg_hello
    call puts16

.code16
halt:
    cli
    hlt
    jmp halt


.set KEYBOARD_DATA_PORT,         0x60
.set KEYBOARD_COMMAND_PORT,      0x64
.set KEYBOARD_DISABLE,           0xAD
.set KEYBOARD_ENABLE,            0xAE
.set KEYBOARD_READ_OUTPUT_PORT,  0xD0
.set KEYBOARD_WRITE_OUTPUT_PORT, 0xD1

.code16
A20_enable:
    # TODO: Check if already enabled

    # disable keyboard
    call A20_wait_input
    mov al, KEYBOARD_DISABLE
    out KEYBOARD_COMMAND_PORT, al

    # read control output port and flush output buffer
    call A20_wait_input
    mov al, KEYBOARD_READ_OUTPUT_PORT
    out KEYBOARD_COMMAND_PORT, al

    call A20_wait_output
    in al, KEYBOARD_DATA_PORT
    push ax


    call A20_wait_input
    mov al, KEYBOARD_WRITE_OUTPUT_PORT
    out KEYBOARD_COMMAND_PORT, al

    # turn on A20 bit
    call A20_wait_input
    pop ax
    or al, 2
    out KEYBOARD_DATA_PORT, al

    # enable keyboard
    call A20_wait_input
    mov al, KEYBOARD_ENABLE
    out KEYBOARD_COMMAND_PORT, al

.code16
A20_wait_input:
    in al, KEYBOARD_COMMAND_PORT
    test al, 2
    jnz A20_wait_input
    ret

.code16
A20_wait_output:
    in al, KEYBOARD_COMMAND_PORT
    test al, 1
    jnz A20_wait_output
    ret

.code32
puts:
    push esi
    push eax
.loop:
    lodsb
    or al, al
    jz .done

    mov ah, 0x0e
    int 0x10

    jmp .loop
.done:
    pop ax
    pop si
    ret

.code16
puts16:
    push si
    push ax
.loop_puts:
    lodsb
    or al, al
    jz .done_puts

    mov ah, 0x0e
    int 0x10

    jmp .loop
.done_puts:
    pop ax
    pop si
    ret

.halt:
    jmp .halt

# https://en.wikipedia.org/wiki/Segment_descriptor
GDT_start:
    null_descriptor:
        .quad 0
    code_descriptor:
        .word 0xffff # limit
        .word 0 # base address (0-16)
        .byte 0 # base address (16-23)
        .byte 0b10011010 # present, ring 0, code segment, executable, direction 0, readable
        .byte 0b11001111 # granularity (4k pages, 32-bit protected mode), limit (16-19)
        .byte 0 # base address (24-31)
    data_descriptor:
        .word 0xffff # limit
        .word 0 # base address (0-16)
        .byte 0 # base address (16-23)
        .byte 0b10010010 # present, ring 0, data segment, executable, direction 0, readable
        .byte 0b11001111 # granularity (4k pages, 32-bit protected mode), limit (16-19)
        .byte 0 # base address  (24-31)
    code16_descriptor:
        .word 0xffff # limit
        .word 0 # base address (0-16)
        .byte 0 # base address (16-23)
        .byte 0b10011010 # present, ring 0, code segment, executable, direction 0, readable
        .byte 0b00001111 # granularity (1b pages, 16-bit real mode?), limit (16-19)
        .byte 0 # base address (24-31)
    data16_descriptor:
        .word 0xffff # limit
        .word 0 # base address (0-16)
        .byte 0 # base address (16-23)
        .byte 0b10010010 # present, ring 0, data segment, executable, direction 0, readable
        .byte 0b00001111 # granularity (1b pages, 16-bit real mode?), limit (16-19)
        .byte 0 # base address (24-31)
GDT_end:

GDT_descriptor:
    .word GDT_end - GDT_start -1
    .long GDT_start

msg_hello: .ascii "Hello world!\r\n\0"
#  'Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!','Hello world!',0

msg_enter_setup: .ascii "Enter setup\r\n\0"
    
