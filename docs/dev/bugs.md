
Keep the following in mind:
- Any code may be interupted at any time.
- Interupting while a lock is held could cause deadlocks if the interrupt routine wants the deadlock.



Guidelines

- Minimal code in interrupt routine. The code that is there should preferably not lock anything.
- If you do need locks then consider disabling interrupts.

