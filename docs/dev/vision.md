I just implemented message passing and shared memory. Works nicely.

With that done i can continue with a terminal application and a compositor and the protocols between them.

What I value in an operating system is:
- reliability. No sudden updates, shutdowns. An application crashes (compositor, audio server) then it can restart and all clients reconnect.
- reproducibility. environment variables, installing packages and preparing configs in the exact same way on ubuntu is tedious (nixos is great for this).
- Performance. No laggy window dragging, missed key inputs completely unresponsive system when application is throttling CPU and RAM and DISK. This sucks and OS should handle it better. hardware is getting better while operating systems don't utilize it. (not strange of course, OS has to implement driver after all).
- Capabilities. Security at the core. capability syscalls to ask for functionality and check the program has access to. User can grant capability. This means downloaded malware is not as dangerous because it doesn't have full access to RAM, and whole file system.
- Snapshots. Taking a snapshot of the whole system state and restoring at a later point would be sick. A bit unrealistic because there's so much stuff to save and applications will probably break.
- Snappy, quick, flexible GUI. ALT+TAB windows on Windows is slow. Tiling managers on linux and workspaces are great. Question is how to go beyond this to more greatness. I can't help but awe at graphs and transparent windows you see in sci-fi movies.
- Most important of all simplicity. If an operating system is small and simple then it is easy to code for, easy to maintain, easy to expand, easy to reason about security, harder to introduce bugs, and in general more performant because less stuff going on.

A good quote from ChatGPT (or wherever it hallucinated it from): "What guarantees can I make so the desktop remains responsive when everything else is on fire?"


What you want to do in an OS

- Watch videos
- Browse the web
- Play games
- Write text to file in an editor
- Listen to music
- Record audio
- Record screen
- Move around files
- Configure OS (theme, font, user)
- Download applications (compilers, browsers, games)
- Write driver for new hardware like VR trackers, webcam.
