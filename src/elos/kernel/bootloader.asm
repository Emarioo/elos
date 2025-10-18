[org 0x7c00]
[bits 16]

; FAT12 header
jmp short start
nop

bdb_oem:                    db 'MSWIN4.1' ; 8 bytes
bdb_bytes_per_sector:       dw 512
bdb_sectors_per_cluster:    db 1
bdb_reserved_sectors:       dw 1
bdb_fat_count:              db 2        ; number file allocation tables
bdb_dir_entries_count:      dw 0E0h     ; number root directory entries (224)
bdb_total_sectors:          dw 2880     ; 2880 * 512 = 1400 KB
bdb_media_descriptor_type:  db 0F0h
bdb_sectors_per_fat:        dw 9
bdb_sectors_per_track:      dw 18
bdb_heads:                  dw 2
bdb_hidden_sectors:         dd 0
bdb_large_sector_count:     dd 0

; extended boot record
ebr_drive_number:           db 0
                            db 0    ; reserved
ebr_signature:              db 29h
ebr_volume_id:              db 12h, 34h, 56h, 78h   ; serial number
ebr_volume_label:           db 'ELOS       '        ; 11 bytes, space padding
ebr_system_id:              db 'FAT12   '           ; 8 bytes

FAT_ADDR EQU 0x7E00

KERNEL_ADDR EQU 0x8000

LAST_CLUSTER_MIN EQU 0xFF8

start:
    mov ax, 0
    mov ds, ax
    mov es, ax

    mov ss, ax
    mov sp, 0x7C00

    ; some bios might start us at 07C0:0000 so we make sure we start at 0000:7C00
    push es
    push word .after
.after:

    mov [ebr_drive_number], dl ; bios sets dl to drive number

    mov si, msg_loading
    call puts

    ; #######################
    ;   LOAD KERNEL CODE
    ; #########################
    ; For simplicity, we assume the kernel is the first entry
    ; in the root directory as well as the first cluster in
    ; the data part of the FAT12 format.

    ; Read file allocation table
    mov ax, 1           ; index of physical sector
    mov cl, 1           ; sectors to read
    mov bx, FAT_ADDR    ; where to load data
    call disk_read

    mov si, FAT_ADDR
    add si, 3 ; skip first two entries in FAT (3 because each entry is 12 bits)
    mov cx, 1 ; number of clusters, start with one

    ; Read entries in file allocation table.
    ; We assume the first entry is the kernel and we assume the
    ; clusters of the kernel code is contiguous.
.loop:
    mov ax, [si] ; load entry from FAT

    ; Extract the next cluster number, a little complex because each entry is 12 bits
    test cx, 1
    jnz .even
.odd:
    shr ax, 4
    add si, 2
    jmp .end_even
.even:
    and ax, 0xFFF
    add si, 1
.end_even:

    ; Is the cluster number we read the end, if so then we can stop counting.
    cmp ax, LAST_CLUSTER_MIN
    jz .end
    cmp ax, 0
    jz .end

    inc cx
    jmp .loop
.end:

    mov ax, 33             ; index of physical sector
    mov bx, KERNEL_ADDR    ; where to load data
.loop_disk:
    push cx ; save

    mov cx, 1              ; sectors to read, we do one at a time for simplicity
    call disk_read

    pop cx ; load

    dec cx      ; one less sector to read
    inc ax      ; move forward physical sector index, once again, we assume kernel code is contiguous
    add bx, 512 ; move forward pointer where to load data

    or cx, cx   ; Repeat if we have more kernel code?
    jnz .loop_disk
.end_disk:

    mov si, msg_done
    call puts

    ; Call second stage of bootloading where we have more code.
    ; (shouldn't be named KERNEL_ADDR)
    jmp KERNEL_ADDR

    cli
    hlt

floppy_error:
    mov si, msg_read_failed
    call puts
    jmp wait_key_and_reboot

wait_key_and_reboot:
    mov ah, 0
    int 16h         ; wait for keypress
    jmp 0FFFFh: 0   ; jump to beginning of BIOS, should reboot

    cli ; disable interrupts
    hlt

.halt:
    cli ; disable interrupts,   
    jmp .halt

; Convert logical address to disk location
; Arguments
;   ax: LBA address
; Returns
;   cx [0-5] :  sector number
;   cx [6-15]:  track
;   dh       :  head
lba_to_chs:
    push ax
    push dx

    xor dx, dx
    div word [bdb_sectors_per_track]    ; ax = lba / sectors_per_track
                                        ; dx = lba % sectors_per_track
    inc dx  ; dx = lba % sectors_per_track + 1 = sector
    mov cx, dx

    xor dx, dx
    div word [bdb_heads]    ; ax = lba / sectors_per_track / heads = track
                            ; dx = lba / sectors_per_track % heads = head
    mov dh, dl ; dh = head
    mov ch, al ; ch = cylinder (lower 8 bits)
    shl ah, 6
    or  cl, ah  ; 2 upper bits of cylinder in cl

    pop ax
    mov dl, al ; restor dl, dh is used in return
    pop ax
    ret

; Read sectors from disk
; Arguments:
;  ax: lba address
;  cl: sectors to read
;  dl: drive number
;  es:bx: memory address to store read data
disk_read:
    
    push di
    push dx
    push cx
    push bx
    push ax

    push cx ; save number of sectors to read
    call lba_to_chs
    pop ax  ; pop sectors to read

    mov ah, 02h
    mov di, 3   ; retry count

.retry:
    pusha
    stc         ; set carry
    int 13h
    jnc .done   ; success if carry was cleared by interrupt
    
    popa
    call disk_reset

    dec di
    test di, di
    jnz .retry
.fail:
    jmp floppy_error

.done:
    popa

    pop ax
    pop bx
    pop cx
    pop dx
    pop di

    ret

; Resets disk controller
; Arguments:
;   dl: drive number
disk_reset:
    pusha
    mov ah, 0
    stc
    int 13h
    jc floppy_error
    popa
    ret

; Write string to screen
; Arguments:
;   ds:si - pointer to string
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

; mostly for debugging
; Arguments: si
putc:
    push si
    push ax

    mov ax, si

    mov ah, 0eh
    int 0x10

    pop ax
    pop si
    ret

msg_loading:        db 'Loading...', 13, 10, 0
msg_done:           db 'Loading done.', 13, 10, 0
msg_read_failed:    db 'Disk read failed', 13, 10, 0

times 510-($-$$) db 0
db 0x55, 0xaa
