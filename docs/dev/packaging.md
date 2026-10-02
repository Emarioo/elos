

This project produces an Operating System, and default user packages.

A release contains OS and user packages.

A test contains OS and user packages and additionally test program.

The Operating System can be compiled and packaged with different settings.
This is specified in a configuration.

We want to package and build the OS on different platforms: Windows, Linux, and ELOS.
This means we need a universal way to declare the configuration and build the system.

|Platform|EFI Bootloader|ELOS Source (kernel and user apps)|
|-|-|-|
|Windows (WSL for now)|-|-|
|Linux|GCC|GCC|
|ELOS|GCC|GCC|



Packages at `/pkg`.
Multiple versions of the same package.
They can be prebuilt binaries, closed source.
Or they can be reproducible and rebuildable.
Hotreloading very doable?





