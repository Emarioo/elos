
Suspected bugs:
- does laptop not have features i assume it has? i need to check cpuid properly.
- netboot server gets warning: ACK duplicates?



Keep the following in mind:
- Any code may be interupted at any time.
- Interupting while a lock is held could cause deadlocks if the interrupt routine wants the deadlock.
- Writing/reading to registers may need to be done in a specific order or the hardware CPU thing won't get setup.
  Maybe you have to read/write multiple times even? The code you see in the editor and how it executes and affect real hardware
  are every different things!
- Be aware of programs compiled assuming red zone.

Guidelines

- Minimal code in interrupt routine. The code that is there should preferably not lock anything.
- If you do need locks then consider disabling interrupts.

