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

# @dataclass
# class Options:
#     verbose: bool

def main():
    global VERBOSE

    # CONFIG
    run = True
    gdb = False

    # options = Options()
    
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
        else:
            print(f"Unknown argument '{arg}'")
            exit(1)

    # COMPILE
    # if os.path.exists("bin"):
    #     shutil.rmtree("bin")


    build_elos("bin/elos.img")

    # create_floppy()

    # compile_kernel()

    
    # EXECUTE
    if run:
        # os.system("qemu-system-i386 -fda bin/floppy.img")
        flags = ("-drive format=raw,file=bin/elos.img "
                 "-boot a "
                 "-m 64M "
                 "-no-reboot "
                 "-serial stdio "
                 "-monitor none "
                 "-s "
                 + ("-S " if gdb else "")
                #  "-display none "
        )
        cmd("qemu-system-i386 " + flags)
        # @REM goto RUN
        # @REM qemu-system-x86_64 -fda bin/floppy.img
        # qemu-system-i386 -fda bin/floppy.img

        # @REM Debugging
        # @REM qemu-system-i386 -s -S -fda bin/floppy.img
        # @REM gdb -ex "
        # @REM target remote localhost:1234
        # @REM " -ex "set architecture i8086" -ex "break *0x7c00" -ex "continue"
        # @REM set disassembly-flavor intel

        # @REM     @REM qemu-system-x86_64 bin/boot.bin
        # @REM     qemu-system-x86_64 -fda bin/floppy.img

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

    cmd(f"{AS} -g src/elos/kernel/bootloader.s -o bin/elos/bootloader.o")
    cmd(f"{AS} -g src/elos/kernel/setup.s -o bin/elos/setup.o")
    cmd(f"{LD} -T src/elos/kernel/kernel.ld bin/elos/bootloader.o bin/elos/setup.o -o bin/elos.elf") # --oformat=binary

    cmd(f"objcopy -O binary bin/elos.elf {output}")
    # cmd(f"{LD} -T src/elos/kernel/kernel.ld --oformat=binary -o {output} bin/elos/setup.o bin/elos/bootloader.o")

def create_floppy():
    FLAGS = "-g -Iinclude -Wno-builtin-declaration-mismatch -Isrc/fs"
    SRC = "src/fs/fat.c src/fs/make_fat.c src/floppy/create_floppy.c"
    cmd(f"{CC} {FLAGS} {SRC} -o bin/create_floppy.exe")
    cmd("bin/create_floppy.exe")

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