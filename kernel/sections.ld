
ENTRY(_start)

SECTIONS {
    
    # @IMPORTANT The text and data addresses are also hardcoded in cpu.c
    # where we setup interrupt and global descriptor tables.

    . = 0x00100000; /* 1 MB */
    __text_start = .;

    .text ALIGN(4K) : {
        *start.o(.text)
        *(.text*)
    }

    . = 0x00200000; /* 1 MB */
    __data_start = .;
    
    .rodata ALIGN(4K) : {
        __rodata_start = .;
        *(.rodata*)
        __rodata_end = .;
    }

    .data ALIGN(4K) : {
        __data_start = .;
        *(.data*)
        __data_end = .;
    }

    .bss ALIGN(4K) : {
        __bss_start = .;
        *(.bss*)
        __bss_end = .;
    }

}
