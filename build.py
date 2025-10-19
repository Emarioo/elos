#!/usr/bin/env python3

'''
This script compiles all tools/binaries by default

The following tools/binaries exist:
- elos.img

'''

import os, sys, platform, shutil, shlex
from dataclasses import dataclass

# TODO: Linux

########################
#      CONSTANTS
########################

AS = "as"
CC = "gcc"
LD = "ld"

VERBOSE = False

def main():
    global VERBOSE

    # CONFIG
    run       = False
    gdb       = False
    vbox      = False
    usb       = False

    argi = 1
    while argi < len(sys.argv):
        arg = sys.argv[argi]
        argi += 1

        if arg == "-h" or arg == "--help":
            print("cat build.py")
        elif arg == "-v" or arg == "--verbose":
            VERBOSE = True
        elif arg == "run":
            run = True
        elif arg == "gdb":
            run = True
            gdb = True
        elif arg == "vbox":
            run = False
            vbox = True
        elif arg == "usb":
            run = False
            usb = True
        else:
            print(f"Unknown argument '{arg}'")
            exit(1)

    if len(sys.argv) <= 1:
        run = True

    if vbox:
        build_elos("bin/elos.img")
        cmd("dd if=bin/elos.img of=bin/elos_padded.img bs=1M count=64 conv=sync")
        vdi_path = "/mnt/d/vms/elos.vdi"
        cmd(f"rm -f {vdi_path}")
        cmd(f"VBoxManage convertfromraw bin/elos_padded.img {vdi_path} --format VDI")
    elif usb:
        build_elos("bin/elos")
        build_image("bin/elos.img")
        # cmd("dd if=bin/elos.img of=bin/elos_padded.img bs=1M count=64 conv=sync")
    else:
        build_elos("bin/out_elos")
        # build_image("bin/elos", "bin/elos.img")
        build_iso("bin/out_elos", "bin/elos.iso")

    if run and False:
        # flags = ("-drive format=raw,file=bin/elos.bin "
        flags = (
            # f"-drive format=raw,file=bin/elos.bin,if=floppy "
            f"-drive format=raw,file=bin/elos.img "
             "-boot c "
            #  "-m 64M "
            #  "-no-reboot "
            #  "-serial stdio "
            #  "-monitor none "
                "-s "
                + ("-S " if gdb else "")
            #  "-display none "
        )
        cmd("qemu-system-i386 " + flags)

def build_elos(output: str):
    os.makedirs("bin", exist_ok=True)

    # WARN = "-fno-builtin -fno-unwind-tables -fno-exceptions -fno-asynchronous-unwind-tables -Wno-builtin-declaration-mismatch"
    # FLAGS = "-nostdlib -nostartfiles -nodefaultlibs -Iinclude"

    # SRC = "src/elos/kernel/kernel.c src/fs/fat.c"
    # OBJ = ""
    # for src in SRC:
    #     obj = f"bin/int/{src[4:-2]}.o"
    #     os.makedirs(os.path.dirname(obj), exist_ok=True)
    #     cmd(f"{CC} {FLAGS} {WARN} -c {src} -o {obj}")
    #     OBJ += obj + " "

    # cmd(f"{LD} -T src/elos/kernel/linker.ld -s {OBJ} -o bin/elos/kernel.bin")

    os.makedirs(output, exist_ok=True)

    # cmd(f"{AS} -g src/elos/kernel/bootloader.s -o bin/elos/bootloader.o")
    # cmd(f"{AS} -g src/elos/kernel/setup.s -o bin/elos/setup.o")

    # cmd(f"{LD} -T src/elos/kernel/kernel.ld bin/elos/bootloader.o -o bin/elos/bootloader.elf")
    # cmd(f"{LD} -T src/elos/kernel/kernel.ld bin/elos/setup.o -o bin/elos/setup.elf")
    # cmd(f"objcopy -O binary bin/elos/bootloader.elf bin/elos/bootloader.bin")
    # cmd(f"objcopy -O binary bin/elos/setup.elf bin/elos/setup.bin")

    # cmd(f"{LD} -T src/elos/kernel/kernel.ld bin/elos/setup.o bin/elos/bootloader.o -o bin/elos.elf")


    ##############
    #    UEFI
    ##############
    INT_DIR     = "bin/elos_int"
    INC_EFI_DIR = "/usr/include/efi"
    shutil.rmtree(output)
    os.makedirs(f"{output}/EFI/BOOT", exist_ok=True)
    os.makedirs(INT_DIR, exist_ok=True)

    CC = "x86_64-w64-mingw32-gcc"
    OBJS = f"{INT_DIR}/main.o {INT_DIR}/data.o"
    CFLAGS = f"-ffreestanding -I{INC_EFI_DIR} -I{INC_EFI_DIR}/x86_64 -I{INC_EFI_DIR}/protocol -Isrc/elos/efi"
    cmd(f"{CC} {CFLAGS} -c -o {INT_DIR}/main.o src/elos/efi/main.c")
    cmd(f"{CC} {CFLAGS} -c -o {INT_DIR}/data.o src/elos/efi/data.c")
    cmd(f"{CC} -nostdlib -Wl,-dll -shared -Wl,--subsystem,10 -e efi_main -o {output}/EFI/BOOT/BOOTX64.EFI {OBJS}")


    #########################################
    #    LEGACY BIOS MBR FAT16 QEMU VBOX
    #########################################
    # cmd(f"{AS} -g src/elos/kernel/mbr_bootloader.s -o bin/elos/mbr_bootloader.o")
    # cmd(f"{AS} -g src/elos/kernel/setup.s -o bin/elos/setup.o")
    # cmd(f"{LD} -T src/elos/kernel/kernel.ld bin/elos/mbr_bootloader.o -o bin/elos/mbr_bootloader.elf")
    # cmd(f"{LD} -T src/elos/kernel/kernel.ld bin/elos/setup.o -o bin/elos/setup.elf")
    # cmd(f"objcopy -O binary bin/elos/mbr_bootloader.elf bin/elos/mbr_bootloader.bin")
    # cmd(f"objcopy -O binary bin/elos/setup.elf bin/elos/setup.bin")
    # cmd(f"{LD} -T src/elos/kernel/kernel.ld bin/elos/setup.o bin/elos/mbr_bootloader.o -o bin/elos.elf")
    
    # Broke, i changed create_image behaviour
    # build_image()
    # cmd(f"bin/create_image.exe {output}")

    # cmd(f"{LD} -T src/elos/kernel/kernel.ld bin/elos/bootloader.o bin/elos/setup.o -o bin/elos.elf")
    # cmd(f"objcopy -O binary bin/elos.elf {output}")

def build_image(os_dir, path):
    FLAGS = "-g -Iinclude -Isrc -Wno-builtin-declaration-mismatch"
    SRC = "src/fs/fat.c src/tools/make_fat.c src/tools/create_image.c"
    cmd(f"{CC} {FLAGS} {SRC} -o bin/create_image.exe")
    cmd(f"bin/create_image -o {path} {os_dir}")

def build_iso(os_dir, path):
    # cmd(f"genisoimage -o {path} {os_dir}")
    FAT_PATH = "bin/elos_int/fat.img"
    cmd(f"dd if=/dev/zero of={FAT_PATH} bs=1k count=1440 conv=fsync")
    cmd(f"mformat -i {FAT_PATH} -f 1440 ::")
    cmd(f"mmd -i {FAT_PATH} ::/EFI")
    cmd(f"mmd -i {FAT_PATH} ::/EFI/BOOT")
    cmd(f"mcopy -i {FAT_PATH} {os_dir}/EFI/BOOT/BOOTX64.EFI ::/EFI/BOOT")
    shutil.copy(FAT_PATH, f"{os_dir}/fat.img")
    # cmd(f"mkisofs -o {path} -eltorito-alt-boot -e fat.img -no-emul-boot -boot-load-size 4 -boot-info-table {os_dir}")
    cmd(f"xorriso -as mkisofs -R -f -e fat.img -no-emul-boot -o bin/elos.iso {os_dir}")

def cmd(c):
    if platform.system() == "Windows":
        strs = shlex.split(c)
        if strs[0].startswith("./"):
            strs[0] = strs[2:]
        strs[0] = strs[0].replace('/', "\\")
        c = shlex.join(strs)
    
    if VERBOSE:
        print(c, file=sys.stderr)
    if platform.system() == "Linux":
        err = os.system(c) >> 8
    else:
        err = os.system(c)
    if err:
        if not VERBOSE:
            print("ERR",c)
        exit(1)

if __name__ == "__main__":
    main()