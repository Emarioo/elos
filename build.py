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
    iso       = False

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
        # elif arg == "usb":
        #     run = False
        #     usb = True
        elif arg == "iso":
            iso = True
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
    # elif usb:
    #     build_elos("bin/elos")
    #     build_image("bin/elos.img")
        # cmd("dd if=bin/elos.img of=bin/elos_padded.img bs=1M count=64 conv=sync")
    else:
        build_elos("bin/elos")
        build_image("bin/elos", "bin/elos.img")
        build_iso("bin/elos", "bin/elos.iso")

    if run:
        # QEMU FAT16
        # flags = ("-drive format=raw,file=bin/elos.bin "
        # flags = (
        #     # f"-drive format=raw,file=bin/elos.bin,if=floppy "
        #     # f"-drive format=raw,file=bin/elos.img "
        #     f"-drive format=raw,file=bin/elos.img "
        #      "-boot c "
        #     #  "-m 64M "
        #     #  "-no-reboot "
        #     #  "-serial stdio "
        #     #  "-monitor none "
        #         "-s "
        #         + ("-S " if gdb else "")
        #     #  "-display none "
        # )
        # cmd("qemu-system-i386 " + flags)

        # TODO: DON'T HARDCODE PATHS
        ovmf_dir = "/usr/share/ovmf"
        shutil.copy(f"{ovmf_dir}/OVMF.fd", "bin/OVMF.fd")
        flags = (
            f"-L {ovmf_dir}/ "
            f"-drive format=raw,file=bin/OVMF.fd,if=pflash "   # -pflash (but without warnings)
            f"-drive format=raw,file=bin/elos.img "            # -hda
            # f"-nographic "
             "-s "
            + ("-S " if gdb else "")
        )
        cmd("qemu-system-x86_64 " + flags)

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
    INT_DIR     = "bin/int"
    INC_EFI_DIR = "/usr/include/efi"
    shutil.rmtree(output)
    os.makedirs(f"{output}/EFI/BOOT", exist_ok=True)
    os.makedirs(INT_DIR, exist_ok=True)

    CC = "x86_64-w64-mingw32-gcc"
    LD = "x86_64-w64-mingw32-ld"
    sources = [
        "src/elos/efi/main.c",
        "src/elos/efi/data.c",

        "src/elos/kernel/kernel.c",
        "src/elos/kernel/frame/frame.c",
    ]
    objects = [ f"{INT_DIR}/{os.path.basename(s)}" for s in sources ]
        
    CFLAGS = f"-ggdb -ffreestanding -fno-asynchronous-unwind-tables -fno-exceptions -I{INC_EFI_DIR} -I{INC_EFI_DIR}/x86_64 -I{INC_EFI_DIR}/protocol -Isrc/elos/efi -Isrc"
    CFLAGS += f" -Wall -Werror"
    CFLAGS += f" -Wno-unused-variable"

    for s,o in zip(sources,objects):
        cmd(f"{CC} {CFLAGS} -c -o {o} {s}")

    OBJS = " ".join(objects)

    cmd(f"{LD} -nostdlib --dll --image-base=0  -shared --subsystem 10 -e efi_main -o {output}/EFI/BOOT/BOOTX64.EFI {OBJS}")
    shutil.copy(f"{output}/EFI/BOOT/BOOTX64.EFI", "bin/elos.elf")
    cmd(f"objcopy --only-keep-debug bin/elos.elf")


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

def build_image(os_dir, img_path):
    # FLAGS = "-g -Iinclude -Isrc -Wno-builtin-declaration-mismatch"
    # SRC = "src/fs/fat.c src/tools/make_fat.c src/tools/create_image.c"
    # cmd(f"{CC} {FLAGS} {SRC} -o bin/create_image.exe")
    # cmd(f"bin/create_image -o {path} {os_dir}")

    # NOTE: Image does not have MBR or GPT
    #   We need to build mkgpt or make our own create_gpt.c

    FAT_PATH = "bin/int/fat.img"

    # TODO: Dynamically increase size (currently 1.4M)
    cmd(f"dd if=/dev/zero of={FAT_PATH} bs=1k count=1440 conv=fsync")
    cmd(f"mformat -i {FAT_PATH} -f 1440 ::")
    cmd(f"mmd -i {FAT_PATH} ::/EFI")
    cmd(f"mmd -i {FAT_PATH} ::/EFI/BOOT")
    cmd(f"mcopy -i {FAT_PATH} {os_dir}/EFI/BOOT/BOOTX64.EFI ::/EFI/BOOT")
    cmd(f"mkgpt -o {img_path} --image-size 4096 --part {FAT_PATH} --type system")

def build_iso(os_dir, path):
    FAT_PATH = "bin/int/fat.img"
    cmd(f"dd if=/dev/zero of={FAT_PATH} bs=1k count=1440 conv=fsync")
    cmd(f"mformat -i {FAT_PATH} -f 1440 ::")
    cmd(f"mmd -i {FAT_PATH} ::/EFI")
    cmd(f"mmd -i {FAT_PATH} ::/EFI/BOOT")
    cmd(f"mcopy -i {FAT_PATH} {os_dir}/EFI/BOOT/BOOTX64.EFI ::/EFI/BOOT")
    shutil.copy(FAT_PATH, f"{os_dir}/fat.img")
    cmd(f"xorriso -as mkisofs -R -f -e fat.img -no-emul-boot -o {path} {os_dir}")

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