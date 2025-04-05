#!/usr/bin/python3

import os, sys, platform, shutil

# TODO: Linux

def main():
    # CONFIG
    run = True

    # COMPILE
    shutil.rmtree("bin")
    compile_kernel()
    create_floppy()
    
    # EXECUTE
    if run:
        os.system("qemu-system-i386 -fda bin/floppy.img")
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

def compile_kernel():
    # We may need to specify 32 bit mode
    FLAGS = "-nostdlib -nostartfiles -nodefaultlibs -fno-builtin -fno-unwind-tables -fno-exceptions -fno-asynchronous-unwind-tables -Wno-builtin-declaration-mismatch"
    FLAGS += " -Iinclude"

    FILES = ["kernel/kernel.c", "fs/fat12.c", "c/string.c"]
    OBJS = []
    for f in FILES:
        of = f"bin/{f[:-2]}.o"
        os.makedirs("bin/"+os.path.dirname(f), exist_ok=True)
        cmd(f"gcc {FLAGS} -c src/{f} -o {of}")
        print(of)
        OBJS.append(of)
    OBJS = " ".join(OBJS)

    LDFLAGS = "-s"
    cmd(f"ld -T src/kernel/linker.ld {LDFLAGS} {OBJS} -o bin/kernel.bin")

    cmd("nasm src/kernel/bootloader.asm -f bin -o bin/bootloader.bin")
    cmd("nasm src/kernel/setup.asm -f bin -o bin/setup.bin")

def create_floppy():
    FLAGS = "-g -Iinclude -Wno-builtin-declaration-mismatch"
    FILES = ["floppy/create_floppy.c", "fs/fat12.c"]
    FILES = " ".join(["src/"+f for f in FILES])
    cmd(f"gcc {FLAGS} {FILES} -o bin/create_floppy.exe")
    start("bin/create_floppy")

def start(program, args = ""):
    if platform.system() == "Windows":
        program = program.replace('/', "\\")
        os.system(f"{program} {args}")
    else:
        os.system(f"./{program} {args}")

def cmd(c):
    err = os.system(c)
    if err:
        exit(1)

if __name__ == "__main__":
    main()