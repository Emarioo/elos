
ENTRY(_start)

SECTIONS {
    
    # @IMPORTANT The text and data addresses are also hardcoded in physical_memory.c.
    #   Where we mark addresses as in use.

    . = 0x200000;
    __kernel_start = .;

    .text ALIGN(4K) : {
        __text_start = .;
        *start.o(.text)
        *(.text*)
        __text_end = .;
    }

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

    __kernel_end = 0x400000; # This is silly because .bss or other section can outgrow the end and we won't notice at link time but by an obscure crash.

    __stack_start = 0x1800000;
    __stack_end   = 0x1A00000;

}
