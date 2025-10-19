
# Immediate route
- [x] Use Hard Drive, Master Boot Record, Partition table instead of floppy **UPDATE:** still works in QEMU, booting in VirtualBox or real hardware still doesn't work.
- [] Turn raw bootloader/kernel into ISO format.
- [] Boot kernel on real hardware, I want to see colored characters written to the screen.

# Current goals
- [ ] Boot on real hardware, I want to see colored text on the screen.
- [ ] Produce ISO file and load in virtual box. We'd have one hard disk and an optical drive, see how that works.

# Future goals
- [x] Load second bootloader with more than 512 bytes.
- [ ] Compiler and load kernel in C.
- [ ] Call assembly functions from C code.
- [ ] FAT12 file system.
- [ ] Terminal user interface.
- [ ] Terminal text editor.

## Loading kernel
I have C code
I have assembly code
I want to compile it and put into a floppy image.
Then read disc sectors and load it into memory.

Compiling C code into a binary blob is harder than it should be.