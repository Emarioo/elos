
.PHONY: all floppy


floppy: bin/floppy.img
boot: bin/boot.bin
kernel: bin/kernel.bin

bin/floppy.img: boot kernel
	gcc src/other/create_floppy.c -o bin/create_floppy.exe
	bin/create_floppy bin/floppy.img 

bin/boot.bin:
	nasm src/boot.asm -f bin -o bin/boot.bin

bin/kernel.bin:
	nasm src/kernel/main.asm -f bin -o bin/kernel.bin

clean:
	del /Q bin