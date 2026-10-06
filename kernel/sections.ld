
ENTRY(_start)

PHYSICAL_KERNEL_BASE = 0x200000;
VIRTUAL_KERNEL_BASE = 0xFFFFFFFF80200000;
KERNEL_OFFSET = VIRTUAL_KERNEL_BASE - PHYSICAL_KERNEL_BASE;

PHDRS
{
    low_boot PT_LOAD FLAGS(5); /* R-X */
    text     PT_LOAD FLAGS(5); /* R-X */
    rodata   PT_LOAD FLAGS(4); /* R-- */
    data     PT_LOAD FLAGS(6); /* RW- */
}

SECTIONS {
    
    # @IMPORTANT The text and data addresses are also hardcoded in physical_memory.c.
    #   Where we mark addresses as in use.

    .low_boot PHYSICAL_KERNEL_BASE :
        AT(PHYSICAL_KERNEL_BASE)
    {
        __low_boot_start = .;
        *(.low_boot)
        __low_boot_end = .;
    } :low_boot

    .text VIRTUAL_KERNEL_BASE + 0x4000 :
        AT(ADDR(.text) - KERNEL_OFFSET)
    {
        __kernel_start = .;
        __text_start = .;
        *start.o(.text)
        *(.text*)
        __text_end = .;
    } :text

    .rodata ALIGN(4K)  :
        AT(ADDR(.rodata) - KERNEL_OFFSET)
    {
        __rodata_start = .;
        *(.rodata*)
        __rodata_end = .;
    } :rodata

    .data ALIGN(4K) :
        AT(ADDR(.data) - KERNEL_OFFSET)
    {
        __data_start = .;
        *(.data*)
        __data_end = .;
    } :data

    .bss ALIGN(4K) :
        AT(ADDR(.bss) - KERNEL_OFFSET)
    {
        __bss_start = .;
        *(.bss*)

        . = ALIGN(4K);

        __bss_end = .;

        __stack_start = .;
        . = . + 0x8000;
        __stack_end   = .;
        
        __kernel_end = .;
    } :data


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
        *(.got*)
        *(.got.plt*)
        *(.iplt*)
        *(.rela*)
        *(.igot.plt*)
    }

}
