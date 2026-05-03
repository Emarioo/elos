We want a debuggable kernel lots of logs and information about what is happening especially when running on real hardware.

In QEMU we have GDB, UART, Display. On a laptop we have a display and that's it. Eventually a network driver which we can send logs and data through. Kernel may crash before anything is sent though.

# What we do to make it debuggable.

1. Fast iteration cycle.
    - Kernel is compiled on dev machine, test machine boots and downloads Kernel from network. No need to move around USB drive. (PXE is an option but requires a specific setup, we use a custom UDP file transfer protocol.)
    - Fast compile times. Lean in heavy on incremental linking.
2. Establish writing to display early.
    - We enable graphics protocol in EFI which gives us a memory area to "draw" on. This area is usable when exiting EFI.
    - PS1 Font embedded in the kernel. Font is parsed into structures usable by the kernels draw functions without allocating any memory. We reserve static memory in the kernel binary's .bss section. If paging or memory code is broken then we can still establish a font.
    - Drawing functions allocate to memory. Pure code that writes to Frame Buffer.
3. All parts of the kernel are able to write to the display.


