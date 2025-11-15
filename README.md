My attempt at kernel/OS development

This project uses GNU tools and MinGW GNU linker isn't a fan of turning PE/COFF to binaries
which is why you need Linux or Windows Subsystem for Linux for this project. Altough for UEFI it's fine.

# Dependencies
These are the dependencies:

- `Python 3.11+`           (for build/test scripts)
- `gcc-mingw-w64-x86-64`   (for EFI application)
- `gcc`                    (for C programs on host and for OS)
- `qemu-system-x86`        (Virtual Machine to run OS)
- `Rufus`                  (To flash USB with OS, Windows)
- `dd`                     (To flash USB with OS, Linux)

We need gcc-mingw-w64-x86-64 to compile EFI applications which handles
some of the startup of the Operating System. EFI applications should
use Windows x64 calling conventions which is why classic `gcc` on Linux
doesn't work.

Everything isn't setup as it should be. We also use mingw to compile OS and kernel code
which maybe isn't what we want in the end.

# Installing (dependencies)
I would love a python script that installs all dependencies on Windows and Linux
but we do not live in an ideal world. Maybe we will in the future.


## Linux (Ubuntu, or WSL)
Assuming you have cloned the repo do these steps:

**Install dependencies (auto)**
```
sudo apt install python3
build.py install
```

**Install dependencies (manual)**
```bash
sudo apt install python3
sudo apt install x86_64-w64-mingw32-gcc
sudo apt install qemu-system-x86 # (also installs qemu-system-x86_64)
```
<!--
Not needed, we include these in the repo 

sudo apt install gnu-efi
sudo apt install ovmf

**Copy headers and other junk**
```bash
mkdir -p extern/efi
mkdir -p extern/ovmf
cp /usr/share/ovmf/OVMF.fd extern/efi/OVMF.fd
cp -r /usr/include/efi extern
```
-->

Then follow the [Generic](#generic) approach, same for Linux and Windows.

## Windows
Assuming you have cloned the repo do the following:
- Install python (3.11+)
- Install MinGW GNU GCC
- Install QEMU x86_64

<!-- Not needed, we include these in the repo 
Then you need EFI headers and OVMF.
The easiest way I've found is to use WSL (ubuntu),
install them in WSL, then copy them to Windows.
```bash
sudo apt install gnu-efi
sudo apt install ovmf
cd /mnt/c/Users/%USER%/Desktop/elos # or where you cloned the repo
mkdir -p extern/efi
mkdir -p extern/ovmf
cp /usr/share/ovmf/OVMF.fd extern/efi/OVMF.fd
cp -r /usr/include/efi extern
```

In the future I may provide the EFI headers in the repoOVMF.fd in the repo or as a git release, or somewhere else you can just download.
For the EFI headers I could simply include them in the repo.
(same day I wrote this I included it)
-->


Then follow the [Generic](#generic) approach, same for Linux and Windows.

## Generic
Now we are in the ideal world. One python script to build OS and start QEMU:
```bash
build.py
# same as 
build.py img run
```

Normally you may have to build mkgpt and install mtools but this projects
implements what we need in C so those aren't needed (for now).


# Running
```bash
# Just run QEMU
build.py run

# Run QEMU with GDB, uses port 1234 by default
build.py gdb
# then start gdb
gdb -x scripts/gdb.txt
```

# Flashing OS to USB drive (WIP)

## Windows (WIP)
At the moment ISO doesn't work on Windows alone.
You need WSL (run `build.py install` there).
This installs xorriso which creates ISO file.

As mentioned earlier, you need Rufus on Windows.
Then run below which will make `bin/elos.iso`
```bash
build.py iso
```

Then open Rufus, select USB drive, select iso file and BAM.
You are good to go.
The operating system should not mess with your storage devices except for
the USB drive unless you tell it to. But there may always be bugs
so backup your computer! (I know you're lazy but please do just to be safe)

At the moment `build.py ISO` is a little broken.

There is a way to use `dd`

## Linux (WIP)
You can use the `dd` command to blit a raw image to a USB drive.
The `of` argument is the block device (USB drive) to use.
MAKE SURE YOU PICK THE RIGHT ONE, looking at the size is a good way
to determine which to use.

```bash
build.py img # "build.py" does this by default
lsblk        # shows block devices
sudo dd if=bin/elos.img of=/dev/sda1 bs=512 conv=fsync
```

## Running OS on actual machine
Shutdown your computer.
Plug your flashed USB drive into the computer.
Boot into BIOS by repeatedly pressing DELETE or F2 (F2 is common on ASUS computers).

Then select your USB in BIOS. BIOSes look different but a common thing to search for is "Boot order" or "Boot priority". It may say UEFI beside it which is correct.
If it says Legacy Boot or CSM then it's probably wrong. Modern computers have UEFI so that shouldn't be a problem.

Then press enter or click and it should boot up.

There is a high chance it doesn't boot, this means my OS doesn't follow standards and there is probably nothing you can do about it.


# File structure

- `docs` contains design decisions, information, specifications
- `include` contains headers for the public interface of software components
- `src` implementation of software components
- `src/elos` operating system, kernel, bootloader
- `src/c` standard library used in ELOS
- `src/fs` file system implementation, used by `tools` and `elos`
- `src/tools` tools for creating kernel images, testing and what not
- `res` resources of type texture and font.
- `image` content for the OS image and ISO, text files, textures, fonts currently included.
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
