What happens when we enter the kernel from EFI application?

EFI app loads kernel image.

We are able to read/write/execute bytes in the loaded kernel image as well as the stack.
EFI app provided us with struct BootAPI which has physical memory regions, address to frame buffer and pointer to RSDT (ACPI).

# Essentials first

1. The absolute first thing is initialize serial output if available.
When other things initialize we log it to serial output.

2. Second we must initialize frame buffer and load a font because we may not have serial output (laptop). Printed text lets us see where kernel froze.

3. Setup GDT, IDT and exception handlers for page fault.

4. Setup page tables. We will want to allocate memory, read ACPI, map MMIO from PCI devices and we must therefore
be able to map virtual and physicla memory by tweaking page tables.

5. Parse ACPI tables. Information for APIC can be found here among other things.

6. Initialize network driver. It lets us send logs to a secondary computer. Not required in QEMU but very useful on laptop where we don't have serial output and where the screen may get clobbered with text not fitting on the screen.

7. Initialize APIC and enable interrupts. Needed later for keyboard presses, incoming network packets, and periodic process rescheduling.

# Useful stuff

1. Initialize PS/2 keyboard.


# User mode

1. Start operating system processes and user space processes.
