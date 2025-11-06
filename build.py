#!/usr/bin/env python3

'''
This script compiles all tools/binaries by default

The following tools/binaries exist:
- elos.img

'''

import os, sys, platform, shutil, shlex, glob, math
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
    elif iso:
        build_elos("bin/elos")
        build_iso("bin/elos", "bin/elos.iso")
    # elif usb:
    #     build_elos("bin/elos")
    #     build_image("bin/elos.img")
        # cmd("dd if=bin/elos.img of=bin/elos_padded.img bs=1M count=64 conv=sync")
    else:
        build_elos("bin/elos")
        build_image("bin/elos", "bin/elos.img")
        # build_iso("bin/elos", "bin/elos.iso")

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
            f"-drive format=raw,file=bin/elos.img,if=ide,media=disk,cache=writeback "            # -hda
            # f"-drive if=none,id=disk0,format=raw,file=bin/elos.img "            # -hda
            # f"-device ahci,id=ahci "
            # f"-device ide-drive,drive=disk0,bus=ahci.0 "
            # f"-nographic "
            "-serial file:kernel.log "
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

    # build_bitmap()


    ##############
    #    UEFI
    ##############
    INT_DIR     = "bin/int"
    INC_EFI_DIR = "extern/efi"
    shutil.rmtree(output)
    # os.makedirs(f"{output}/EFI/BOOT", exist_ok=True)
    os.makedirs(INT_DIR, exist_ok=True)

    CC = "x86_64-w64-mingw32-gcc"
    LD = "x86_64-w64-mingw32-ld"
    sources = [
        "src/elos/efi/main.c",
        "src/elos/efi/data.c",

        "src/elos/kernel/kernel.c",
        "src/elos/kernel/frame/frame.c",
        "src/elos/kernel/frame/font/font.c",
        "src/elos/kernel/frame/font/psf.c",
        "src/elos/kernel/log/print.c",
        "src/elos/kernel/driver/pata.c",
        "src/elos/kernel/driver/pci.c",
        "src/elos/kernel/common/string.c",
        "src/elos/kernel/memory/phys_allocator.c",
        "src/elos/kernel/memory/paging.c",
        "src/elos/kernel/debug/debug.c",

        "res/ascii_bitmap.c", # temporary
    ]
    objects = [ f"{INT_DIR}/{os.path.basename(s)}" for s in sources ]
        
    CFLAGS = f"-ggdb -ffreestanding -fno-asynchronous-unwind-tables -fno-exceptions -I{INC_EFI_DIR} -Iinclude -I{INC_EFI_DIR}/x86_64 -I{INC_EFI_DIR}/protocol -Isrc/elos/efi -Isrc"
    CFLAGS += f" -Wall -Werror -fshort-wchar -Werror=implicit-function-declaration"
    CFLAGS += f" -Wno-multichar"
    CFLAGS += f" -Wno-unused-variable -Wno-unused-function -Wno-unused-but-set-variable"

    # TODO: Multiple threads
    for s,o in zip(sources,objects):
        cmd(f"{CC} {CFLAGS} -c -o {o} {s}")

    OBJS = " ".join(objects)

    EFI_PATH = "bin/bootx64.efi"

    cmd(f"{LD} -nostdlib --dll --image-base=0  -shared --subsystem 10 -e efi_main -o {EFI_PATH} {OBJS}")
    # cmd(f"{LD} -nostdlib --dll --image-base=0  -shared --subsystem 10 -e efi_main -o {output}/EFI/BOOT/BOOTX64.EFI {OBJS}")
    shutil.copy(EFI_PATH, "bin/elos.elf")
    cmd(f"objcopy --only-keep-debug bin/elos.elf")

    # os.makedirs(f"{output}/RES", exist_ok=True)
    # shutil.copy(f"res/PixelOperator.ttf", f"{output}/RES/PIXELOP.TTF")
    # shutil.copy(f"res/Lat2-Terminus16.psf", f"{output}/RES/STDFONT.PSF")
    # shutil.copy(f"res/PixelOperator.ttf", f"{output}/PIXELOP.TTF")
    # shutil.copy(f"res/PixelOperator.ttf", f"{output}/RES/PIXELOP.TTF")

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

# Returns size in kb
def build_fat(FAT_PATH):

    INT_DIR = "bin/image"
    
    # Collect dependencies
    DEPS_SPEC: list[tuple[str,str]] = [
        ("res/Lat2-Terminus16.psf", "RES/STDFONT.PSF"),
        ("bin/bootx64.efi", "EFI/BOOT/BOOTX64.EFI"),
    ]

    DEPS = []
    for d in DEPS_SPEC:
        if d[0].find("*") == -1:
            # File
            os.makedirs(os.path.dirname(d[1]), exist_ok=True)
            DEPS.append(d)
        else:
            # Wild card directory
            for f in glob.glob(d[0], recursive=True):
                outf = os.path.join(d[1], os.path.basename(f))
                os.makedirs(d[1], exist_ok=True)
                DEPS.append((f, outf))

    # Compute file size
    totalSize = 0
    for src, dst in DEPS:
        totalSize += os.path.getsize(src)

    fatSize_kb = math.ceil(math.ceil(totalSize * 1.25) / 1024 / 1024) * 1024
    if fatSize_kb < 4096:
        fatSize_kb = 4096

    print(totalSize, fatSize_kb)

    # Format FAT image
    cmd(f"dd if=/dev/zero of={FAT_PATH} bs=1k count={fatSize_kb} conv=fsync")
    cmd(f"mformat -i {FAT_PATH} ::")
    # cmd(f"mformat -i {FAT_PATH} -f {fatSize_kb} ::") # -f with floppy image size calculation

    # Copy files
    for src, dst in DEPS:
        assert dst[0] != '/', f"{src} -> {dst}"
        
        split = os.path.dirname(dst).split("/")
        acc = ""
        for s in split:
            acc = os.path.join(acc, s)
            cmd(f"mmd -D o -i {FAT_PATH} ::/{acc}")

        cmd(f"mcopy -D o -i {FAT_PATH} {src} ::/{dst}")

        int_dst = os.path.join(INT_DIR, dst)
        os.makedirs(os.path.dirname(int_dst), exist_ok=True)
        shutil.copy(src, int_dst)

    return fatSize_kb

def build_image(os_dir, img_path):
    # FLAGS = "-g -Iinclude -Isrc -Wno-builtin-declaration-mismatch"
    # SRC = "src/fs/fat.c src/tools/make_fat.c src/tools/create_image.c"
    # cmd(f"{CC} {FLAGS} {SRC} -o bin/create_image.exe")
    # cmd(f"bin/create_image -o {path} {os_dir}")

    # NOTE: Image does not have MBR or GPT
    #   We need to build mkgpt or make our own create_gpt.c

    FAT_PATH = "bin/int/fat.img"


    # cmd(f"dd if=/dev/zero of={FAT_PATH} bs=1k count=1440 conv=fsync")
    # cmd(f"mformat -i {FAT_PATH} -f 1440 ::")
    # cmd(f"mmd -i {FAT_PATH} ::/EFI")
    # cmd(f"mmd -i {FAT_PATH} ::/EFI/BOOT")
    # cmd(f"mmd -i {FAT_PATH} ::/RES")
    # cmd(f"mcopy -i {FAT_PATH} {os_dir}/EFI/BOOT/BOOTX64.EFI ::/EFI/BOOT/")
    # cmd(f"mcopy -i {FAT_PATH} {os_dir}/RES/PIXELOP.TTF ::/RES/")
    # cmd(f"mcopy -i {FAT_PATH} {os_dir}/RES/STDFONT.PSF ::/RES/")
    # cmd(f"mkgpt -o {img_path} --image-size 4096 --part {FAT_PATH} --type system")

    size_kb = build_fat(FAT_PATH) + 8192
    cmd(f"mkgpt -o {img_path} --image-size {size_kb} --part {FAT_PATH} --type system")

def build_iso(os_dir, path):
    # assert False, "is this code up to date with build_image, missing some fonts or other files maybe?"
    FAT_PATH = "bin/int/fat.img"
    # cmd(f"dd if=/dev/zero of={FAT_PATH} bs=1k count=1440 conv=fsync")
    # cmd(f"mformat -i {FAT_PATH} -f 1440 ::")
    # cmd(f"mmd -i {FAT_PATH} ::/EFI")
    # cmd(f"mmd -i {FAT_PATH} ::/EFI/BOOT")
    # cmd(f"mmd -i {FAT_PATH} ::/RES")
    # cmd(f"mcopy -i {FAT_PATH} {os_dir}/EFI/BOOT/BOOTX64.EFI ::/EFI/BOOT/")
    # cmd(f"mcopy -i {FAT_PATH} {os_dir}/RES/PIXELOP.TTF ::/RES/")
    # cmd(f"mcopy -i {FAT_PATH} {os_dir}/RES/STDFONT.PSF ::/RES/")
    # shutil.copy(FAT_PATH, f"{os_dir}/fat.img")
    # cmd(f"xorriso -as mkisofs -R -f -e fat.img -no-emul-boot -o {path} {os_dir}")

    print("WARNING: iso in Virtual Box is not working at the moment")

    INT_DIR = "bin/image"
    size_kb = build_fat(FAT_PATH)
    cmd(f"xorriso -as mkisofs -R -f -o {path} {INT_DIR}")

    # cmd(f"xorriso -as mkisofs -R -f -e fat.img -no-emul-boot -o {path} {os_dir}")

    # cmd(f"mkgpt -o {img_path} --image-size {size_kb} --part {FAT_PATH} --type system")

def build_bitmap():
    cmd("gcc -o bin/create_bitmap src/tools/create_bitmap.c -Iinclude/vendor -lm")
    cmd("bin/create_bitmap")

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