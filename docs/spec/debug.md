At the moment we put address of image at `0x10008` (marker at `0x10000` with value `0xDEADCE11`)

Set watch point in GDB to check when the value is set. And load ELF file with debug info at the address at `0x10008`.

See https://wiki.osdev.org/Debugging_UEFI_applications_with_GDB