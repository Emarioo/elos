@echo off
setlocal enabledelayedexpansion

SET arg=%1
if !arg!==run (
    @REM goto RUN
    @REM qemu-system-x86_64 -fda bin/floppy.img
    qemu-system-i386 -fda bin/floppy.img
    
    @REM Debugging
    @REM qemu-system-i386 -s -S -fda bin/floppy.img
    @REM gdb -ex "
    @REM target remote localhost:1234
    @REM " -ex "set architecture i8086" -ex "break *0x7c00" -ex "continue"
    @REM set disassembly-flavor intel
) else (
    @REM make

    @REM Create floppy image from C because I am on Windows and don't have other tools.

	nasm src/bootloader.asm -f bin -o bin/bootloader.bin
	nasm src/kernel/main.asm -f bin -o bin/kernel.bin

    gcc -g src/other/create_floppy.c -o bin/create_floppy.exe
	bin\create_floppy



    @REM pushd other
    @REM build.bat
    @REM popd

    @REM nasm -f bin src/boot.asm -o bin/boot.bin
)

@REM if %errorlevel% == 0 (
@REM :RUN
@REM     @REM qemu-system-x86_64 bin/boot.bin
@REM     qemu-system-x86_64 -fda bin/floppy.img
@REM )