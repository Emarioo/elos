.intel_syntax noprefix

# set by linker
.section .boot, "ax"
.code16

.globl boot_sector_start
boot_sector_start:

# FAT12 header
jmp short start
nop

bdb_oem:                    .ascii "MSWIN4.1" # 8 bytes
bdb_bytes_per_sector:       .word 512
bdb_sectors_per_cluster:    .byte 1
bdb_reserved_sectors:       .word 1
bdb_fat_count:              .byte 2        # number file allocation tables
bdb_dir_entries_count:      .word 0x0E0     # number root directory entries (224)
bdb_total_sectors:          .word 2880     # 2880 * 512 = 1400 KB
bdb_media_descriptor_type:  .byte 0xF0
bdb_sectors_per_fat:        .word 9
bdb_sectors_per_track:      .word 18 # These two are updated by load_drive_parameters
bdb_heads:                  .word 2
bdb_hidden_sectors:         .long 0
bdb_large_sector_count:     .long 0

# extended boot record
ebr_drive_number:           .byte 0
                            .byte 0    # reserved
ebr_signature:              .byte 0x29
ebr_volume_id:              .byte 0x12, 0x34, 0x56, 0x78    # serial number
ebr_volume_label:           .ascii "FAT12      "        # 11 bytes, space padding
ebr_system_id:              .ascii "FAT12   "           # 8 bytes

.set FAT_ADDR, 0x7e00

.set KERNEL_ADDR, 0x8000

.set LAST_CLUSTER_MIN, 0xFF8


start:
    mov ax, 0
    mov ds, ax
    mov es, ax

    mov ss, ax
    mov sp, 0x7C00

    # some bios might start us at 07C0:0000 so we make sure we start at 0000:7C00
    push es
    push .after
.after:

    mov [ebr_drive_number], dl # bios sets dl to drive number

    lea si, msg_loading
    call puts

    call load_drive_parameters

    # #######################
    #   LOAD KERNEL CODE
    # #########################
    # For simplicity, we assume the kernel is the first entry
    # in the root directory as well as the first cluster in
    # the data part of the FAT12 format.

    # Read file allocation table
    mov ax, 1           # 0 is master boot record, 1 is fat boot sector in first partition, 2 is File allocation table (specified by create_harddrive.c)
    mov cl, 1           # sectors to read
    mov bx, FAT_ADDR    # where to load data
    call load_sectors

    mov si, FAT_ADDR
    add si, 3 # skip first two entries in FAT (3 because each entry is 12 bits)
    mov cx, 1 # number of clusters, start with one

    # Read entries in file allocation table.
    # We assume the first entry is the kernel and we assume the
    # clusters of the kernel code is contiguous.
.loop:
    mov ax, [si] # load entry from FAT

    # Extract the next cluster number, a little complex because each entry is 12 bits
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

    # Is the cluster number we read the end, if so then we can stop counting.
    cmp ax, LAST_CLUSTER_MIN
    jz .end
    cmp ax, 0
    jz .end

    inc cx
    jmp .loop
.end:

    mov ax, 33             # MBR is 0, The first cluster in FAT12 is 33 (cluster same size as sector in our case)
    mov bx, KERNEL_ADDR    # where to load data
.loop_disk:
    push cx # save

    mov cx, 1              # sectors to read, we do one at a time for simplicity
    call load_sectors

    pop cx # load

    dec cx      # one less sector to read
    inc ax      # move forward physical sector index, once again, we assume kernel code is contiguous
    add bx, 512 # move forward pointer where to load data

    or cx, cx   # Repeat if we have more kernel code
    jnz .loop_disk
.end_disk:

    lea si, msg_done
    call puts

    # Call second stage of bootloading where we have more code.
    # (shouldnt be named KERNEL_ADDR)
    jmp KERNEL_ADDR

    cli
    hlt

floppy_error:
    lea si, msg_read_failed
    call puts
    jmp wait_key_and_reboot

wait_key_and_reboot:
    mov ah, 0
    int 0x16         # wait for keypress
    jmp 0x0FFFF: 0   # jump to beginning of BIOS, should reboot

    cli # disable interrupts
    hlt

.halt:
    cli # disable interrupts,   
    jmp .halt

# Convert logical address to disk location
#  sets these used by lba_to_chs
#    bdb_sectors_per_track
#    bdb_heads
load_drive_parameters:
    pusha
    # TODO: retry?
    
    mov ah, 0x08
    mov di, 0
    mov es, di
    stc
    int 0x13
    jnc 1f
    
    jmp floppy_error
1:
    # DL	number of hard disk drives
    # DH	logical last index of heads = number_of - 1 (because index starts with 0)
    # CX	[7:6] [15:8] logical last index of cylinders = number_of - 1 (because index starts with 0)
    # [5:0] logical last index of sectors per track = number_of (because index starts with 1)
    
    # example (qemu):
    #  dl = 1
    #  dh = 15 (16 heads)
    #  cx = 63 (63 sectors per track, 1 cylinder)

    shr dx, 8
    inc dx
    mov [bdb_heads], dx

    and cx, 0x3F
    mov [bdb_sectors_per_track], cx

    popa
    ret

# Convert logical address to disk location
# Arguments
#   ax: LBA address
# Returns
#   cx [0-5] :  sector number
#   cx [6-15]:  track
#   dh       :  head
lba_to_chs:
    push ax
    push dx

    xor dx, dx
    div word ptr [bdb_sectors_per_track]    # ax = lba / sectors_per_track
                                        # dx = lba % sectors_per_track
    inc dx  # dx = lba % sectors_per_track + 1 = sector
    mov cx, dx

    xor dx, dx
    div word ptr [bdb_heads]    # ax = lba / sectors_per_track / heads = track
                            # dx = lba / sectors_per_track % heads = head
    mov dh, dl # dh = head
    mov ch, al # ch = cylinder (lower 8 bits)
    shl ah, 6
    or  cl, ah  # 2 upper bits of cylinder in cl

    pop ax
    mov dl, al # restor dl, dh is used in return
    pop ax
    ret

# Read sectors from disk
# Arguments:
#  ax: sector number
#  cx: sector count
#  dl: drive number
#  es:bx: memory address to store read data
load_sectors:
    
    push di
    push dx
    push cx
    push bx
    push ax

    push cx # save number of sectors to read
    call lba_to_chs
    pop ax  # pop sectors to read

    mov ah, 0x02
    mov di, 3   # retry count

.retry:
    pusha
    stc         # set carry
    int 0x13
    jnc .done   # success if carry was cleared by interrupt
    
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


# Resets disk controller
# Arguments:
#   dl: drive number
disk_reset:
    pusha
    mov ah, 0
    stc
    int 0x13
    jc floppy_error
    popa
    ret

# Write string to screen
# Arguments:
#   ds:si - pointer to string
puts:
    push si
    push ax
.loop_puts:
    lodsb
    or al, al
    jz .done_puts

    mov ah, 0x0e
    int 0x10

    jmp .loop_puts
.done_puts:
    pop ax
    pop si
    ret

# mostly for debugging
# Arguments: si
putc:
    push si
    push ax

    mov ax, si

    mov ah, 0x0e
    int 0x10

    pop ax
    pop si
    ret

msg_loading:        .ascii "Loading...\r\n\0"
msg_done:           .ascii "Loading done.\r\n\0"
msg_read_failed:    .ascii "Disk read failed\r\n\0"

.fill 512-66 - (. - boot_sector_start), 1, 0 # fill rest with zeros

# Partition table (64 bytes)
.fill 64, 1, 0
# Boot signature (2 bytes)
.byte 0x55, 0xaa
