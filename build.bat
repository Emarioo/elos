@echo off
@REM Compile

@REM pushd other
@REM build.bat
@REM popd

nasm -f bin boot.asm -o bin/boot.bin

if %errorlevel% == 0 (

    @REM Run
    qemu-system-x86_64 bin/boot.bin
)