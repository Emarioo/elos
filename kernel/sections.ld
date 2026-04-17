
ENTRY(_start)

SECTIONS {
    . = 0x00100000; /* 1 MB */

    .text ALIGN(4K) : {
        *start.o(.text)
        *(.text*)
    }

    __rodata_start = .;
    .rodata ALIGN(4K) : {
        *(.rodata*)
    }
    __rodata_end = .;

    .data ALIGN(4K) : {
        *(.data*)
    }

    __bss_start = .;
    .bss ALIGN(4K) : {
        *(.bss*)
    }
    __bss_end = .;

}
