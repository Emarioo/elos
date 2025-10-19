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
        build_elos("bin/elos.img")
        # cmd("dd if=bin/elos.img of=bin/elos_padded.img bs=1M count=64 conv=sync")
    else:
        build_elos("bin/elos.img")

    if run:
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

    os.makedirs("bin/elos", exist_ok=True)

    # cmd(f"{AS} -g src/elos/kernel/bootloader.s -o bin/elos/bootloader.o")
    # cmd(f"{AS} -g src/elos/kernel/setup.s -o bin/elos/setup.o")

    # cmd(f"{LD} -T src/elos/kernel/kernel.ld bin/elos/bootloader.o -o bin/elos/bootloader.elf")
    # cmd(f"{LD} -T src/elos/kernel/kernel.ld bin/elos/setup.o -o bin/elos/setup.elf")
    # cmd(f"objcopy -O binary bin/elos/bootloader.elf bin/elos/bootloader.bin")
    # cmd(f"objcopy -O binary bin/elos/setup.elf bin/elos/setup.bin")

    # cmd(f"{LD} -T src/elos/kernel/kernel.ld bin/elos/setup.o bin/elos/bootloader.o -o bin/elos.elf")

    cmd(f"{AS} -g src/elos/kernel/mbr_bootloader.s -o bin/elos/mbr_bootloader.o")
    cmd(f"{AS} -g src/elos/kernel/setup.s -o bin/elos/setup.o")
    cmd(f"{LD} -T src/elos/kernel/kernel.ld bin/elos/mbr_bootloader.o -o bin/elos/mbr_bootloader.elf")
    cmd(f"{LD} -T src/elos/kernel/kernel.ld bin/elos/setup.o -o bin/elos/setup.elf")
    cmd(f"objcopy -O binary bin/elos/mbr_bootloader.elf bin/elos/mbr_bootloader.bin")
    cmd(f"objcopy -O binary bin/elos/setup.elf bin/elos/setup.bin")
    cmd(f"{LD} -T src/elos/kernel/kernel.ld bin/elos/setup.o bin/elos/mbr_bootloader.o -o bin/elos.elf")

    # cmd(f"{LD} -T src/elos/kernel/kernel.ld bin/elos/bootloader.o bin/elos/setup.o -o bin/elos.elf")
    # cmd(f"objcopy -O binary bin/elos.elf {output}")

    build_image()
    cmd(f"bin/create_image.exe {output}")


def build_image():
    FLAGS = "-g -Iinclude -Isrc -Wno-builtin-declaration-mismatch"
    SRC = "src/fs/fat.c src/tools/make_fat.c src/tools/create_image.c"
    cmd(f"{CC} {FLAGS} {SRC} -o bin/create_image.exe")


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
            print(c)
        exit(1)

if __name__ == "__main__":
    main()