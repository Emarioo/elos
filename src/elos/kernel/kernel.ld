SECTIONS {
    . = 0x7C00;
    .boot : {
        *(.boot)
    }
    . = 0x8000;
    .setup : {
        *(.setup)
    }

    # . = 0xA000;
    # .entry :  { *(.entry)   }
    # .text :   { *(.text)    }
    # .data :   { *(.data)    }
    # .rodata : { *(.rodata)  }
    # .bss :    { *(.bss)     }
}