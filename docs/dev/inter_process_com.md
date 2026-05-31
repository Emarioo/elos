How to do Inter-Process Communication (IPC) safely, fairly, and efficiently.

First why do you want IPC?
- Game applications establish an IPC channel to an audio service which mixes the audio and sends it to OS which sends it to the audio card.
  (mixing happens in user space, OS sends the mixed audio to the hardware audio device)
- Terminals establish IPC/shared memory to a compositor service which renders windows to a frame buffer which is sent to OS and drawn on the monitor.
  (once again window composition in user space, OS draws the final composition to the monitor hardware device, in case of GPU it would look a little different but you get the point)

On a computer applications may provide services which other applications want to talk to.


We have a different types of inter-process communication.
- Message passing channels. Structured in terms of message sent and received. Slow because syscalls.
- Shared memory. Format and protocol is completly up to service process and client process. Efficient because no syscalls once established.
- Network sockets. Ports identify the service. Technically similar to message passing (compared to shared memory). Versatile since it works on local and remote machine.
- File system. Writing and reading files and knowing when writing and reading is done is unstable and very slow, but possible.

Things to consider:
- Can any process connect to any service?
- Can any process spam a channel and the service's message queue fills up meaning other process can't send their messages. A game is sometimes able to send their audio samples resulting in hacky audio (we would use shared memory for this in which case we don't have this problem but you get the point).


Any process that has the IPC capabilty can talk to other processes. Most or if not all will be allowed this. Most programs want to draw and have audio after all.
A service may however have a limit on how many it can handle. 50 apps with their own audio streams that have to be mixed by service every 16 milliseconds (or less) might be the limit.
A service can tell the process that it is being overloaded. It can also disallow new clients.

Spamming can be solved with a to_service_queue per client and not a shared to_service_queue for all clients (probably obvious to most OS developers).
