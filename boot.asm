[org 0x7c00]
[bits 16]

jmp kernel_init

; https://en.wikipedia.org/wiki/Segment_descriptor
GDT_start:
    null_descriptor:
        dd 0
        dd 0
    code_descriptor:
        dw 0xffff ; limit
        dw 0 ; base address
        db 0 ; base addres
        db 0b10011010 ; P, DPL, S, type
        db 0b11001111 ; G, DB, ?, A, limit
        db 0 ; base address
    data_descriptor:
        dw 0xffff ; limit
        dw 0 ; base address
        db 0 ; base addres
        db 0b10010010 ; P, DPL, S, type
        db 0b11001111 ; G, DB, ?, A, limit
        db 0 ; base address
GDT_end:

GDT_descriptor:
    dw GDT_end - GDT_start -1
    dd GDT_start

CODE_SEG equ code_descriptor - GDT_start
DATA_SEG equ data_descriptor - GDT_start

kernel_init:
    cli ; disable interrupts
    lgdt [GDT_descriptor] ; load gdt

    mov eax, cr0
    or eax, 1
    mov cr0, eax
    ; now in protected mode?

    jmp CODE_SEG:start_protected

VIDEO_MEMORY equ 0xb8000

[bits 32]
start_protected:
    ; mov al, 'A'
    ; mov ah, 0x59
    ; mov [VIDEO_MEMORY], ax


; mov eax, 'A'
; push eax
; call testa

; jmp END

; testa:
;     push ebp
;     mov ebp, esp
   
;     mov al, [ebp+8]
;     mov ah, 0x0f
;     mov [VIDEO_MEMORY+160], ax

;     mov esp, ebp
;     pop ebp
;     ret


; mov eax, 4
; mov ebx, 4

; mov ecx, A_STR
push A_STR
push 1
push 1
call draw_text

jmp END

; x, y, str
draw_text:
    push ebp
    mov ebp, esp

    ; [ebp+8] ; x
    ; [ebp+12] ; y
    ; [ebp+16] ; str

    mov eax, [ebp+12]
    mov edx, 160
    mul edx
    mov ecx, eax

    mov eax, [ebp+8]
    mov edx, 2
    mul edx
    add eax, VIDEO_MEMORY
    add eax, ecx

    mov ecx, [ebp+16]

    draw_text_loop:
        ; mov dl, 0
        cmp [ecx], byte 0
        je draw_text_end

        mov dl, [ecx]
        mov dh, 0x0f
        mov [eax], dx

        add eax, 2
        inc ecx
        jmp draw_text_loop

    draw_text_end:

    mov esp, ebp
    pop ebp
    ret

diskNum:
    db 0,0

mov [diskNum], dl ; dl contains current disk number at boot

; LOADING MORE SECTORS
; where to load data (es:bx or es*16 + bx)
; here we load data next to 0x7c00.
mov ax, 0
mov es, ax
mov bx, 0x7e00 ; 0x7c00 + 512 (512 is sector size)

; more required info
mov ah, 2 ; function request?
mov al, 1 ; amount of sector
push ax ; check for error later
mov ch, 0 ; cylinder
mov cl, 2 ; sector
mov dh, 0 ; head
mov dl, [diskNum] ; disk number
int 0x13

jc err10 ; checks carry flag
jmp err1
err10:
mov ax, DISC_ERROR_CARRY
call print_str
err1:

pop bx
cmp al, bl
je err0
mov ax, DISC_ERROR_SECTORS
call print_str
err0:


mov ah, 0x0e
mov al, [0x7e00]
int 0x10

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

; prints string in bx, remember null termination 
print_str:
    mov ah, 0x0e
    mov cl, 0
print_str_loop:
    cmp [bx], cl ; cl to ensure 8-bit operation
    je print_str_end
    mov al, [bx]
    int 0x10
    inc bx
    jmp print_str_loop
print_str_end:
    ret

A_STR:
    db "Some text",0

DISC_ERROR_CARRY:
    db "[ERR DISC] carry flag!",0
DISC_ERROR_SECTORS:
    db "[ERR DISC] bad number of sectors!",0

END:
    jmp $
    times 510-($-$$) db 0
    db 0x55, 0xaa

    times 512 db 'A' ; fill sector with A