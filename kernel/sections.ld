
ENTRY(_start)

KERNEL_BASE = 0x200000;

MEMORY {
    RAM (rwx) : ORIGIN = KERNEL_BASE, LENGTH = 0x2000000
}

SECTIONS {
    
    # @IMPORTANT The text and data addresses are also hardcoded in physical_memory.c.
    #   Where we mark addresses as in use.

    . = KERNEL_BASE;
    __kernel_start = .;

    .text ALIGN(4K) : {
        __text_start = .;
        *start.o(.text)
        *(.text*)
        __text_end = .;
    } > RAM

    .rodata ALIGN(4K) : {
        __rodata_start = .;
        *(.rodata*)
        __rodata_end = .;
    } > RAM

    .data ALIGN(4K) : {
        __data_start = .;
        *(.data*)
        __data_end = .;
    } > RAM
    
    .got : ALIGN(8) {
        *(.got)
        *(.got.*)
    } > RAM

    .got.plt : ALIGN(8) {
        *(.got.plt)
    } > RAM
    
    .iplt : { KEEP(*(.iplt)) } > RAM
    .igot.plt : { KEEP(*(.igot.plt)) } > RAM

    .rela : ALIGN(8) {
        *(.rela*)
    } > RAM

    .bss ALIGN(4K) : {
        __bss_start = .;
        *(.bss*)
        __bss_end = .;
    } > RAM

    . = ALIGN(4K);

    __kernel_end = .;


    /* These addresses seems to work, lower ones i've tried behave strangelu */
    __stack_start = 0x1800000;
    __stack_end   = 0x1A00000;

    .debug_frame    0 : { *(.debug_frame) }
    .debug_line     0 : { *(.debug_line) }
    .debug_info     0 : { *(.debug_info) }
    .debug_abbrev   0 : { *(.debug_abbrev) }
    .debug_aranges  0 : { *(.debug_aranges) }
    .debug_str      0 : { *(.debug_str) }
    .debug_rnglists 0 : { *(.debug_rnglists) }
    .debug_line_str 0 : { *(.debug_line_str) }
    .debug_loclists 0 : { *(.debug_loclists) }


    /DISCARD/ : {
        *(.comment*)
        *(.note*)
    }

}
