My attempt at kernel/OS development

This project uses GNU tools and MinGW GNU linker isn't a fan of turning PE/COFF to binaries
which is why you need Linux or Windows Subsystem for Linux for this project.


# Dependencies

**Always needed:**
- Python 3.11
- GNU gcc, as, ld
- GNU-EFI

**Running in QEMU**
- `sudo apt install qemu-kvm`

**Installing**
- Rufus on windows. It can blit ISO to USB drive.
- `dd` command on Linux where you blit the raw image file to USB drive.


For testing
- 


# Building and running

```bash
# builds and runs kernel/bootloader in QEMU
build.py

# two terminals let you debug it
build.py gdb
gdb-multiarch scripts/gdb.txt
```


# File structure

- `docs` contains design decisions, information, specifications
- `include` contains headers for the public interface of software components
- `src` implementation of software components
- `src/elos` operating system, kernel, bootloader
- `src/c` standard library used in ELOS
- `src/fs` file system implementation, used by `tools` and `elos`
- `src/tools` tools for creating kernel images, testing and what not
- `tests` has tests
- `scripts` useful developer scripts, like gdb qemu commands


# OS layout

```
|---------------------|
|   BIOS / FIRMWARE   |
|---------------------|
         | |
|---------------------|
|     BOOTLOADER      |
|---------------------|
|       KERNEL        |
|---------------------|
|   Operating System  |
|---------------------|
```



# Resources
- [OS Dev, Wiki](https://wiki.osdev.org/Expanded_Main_Page)
- [nanobyte, Youtube](https://www.youtube.com/playlist?list=PLFjM7v6KGMpiH2G-kT781ByCNC_0pKpPN)
- [Inkbox, Youtube](https://www.youtube.com/watch?v=ZFHnbozz7b4)
