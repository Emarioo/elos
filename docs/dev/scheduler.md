Some notes to remember thoughts.

1. Create Kernel threads with stack and entry point. Round robin between them every timer interrupt.

2. Enable multiple processors. Run kernel threads in parallel. Implement spinlock. Make things thread-safe.

3. 


Things i need to do:
- Load executable into memory. Map pages so i can load it.
- Create page table for the user process.
- Go into ring 3. Jump to executable start.
- Implement a user print library that talks to kernel.
- Preempt the processes now and then for other processes


Programs request frequency at which they want to run.
60hz, 144hz, 1000hz, whenever.
OS will check if it's reasonable and allow it or use some minimal amount.