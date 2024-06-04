
ENTRY(start)

SECTIONS {
    . = 0xA000;
    .entry :  { *(.entry)   }
    .text :   { *(.text)    }
    .data :   { *(.data)    }
    .rodata : { *(.rodata)  }
    .bss :    { *(.bss)     }
}