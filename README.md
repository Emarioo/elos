My attempt at kernel/OS development

This project only builds on Linux (Linux has good tools for kernel development).
Personally I use NixOS and Windows Subsystem for Linux.

# Dependencies

These are the dependencies:

- `Python 3.11+`           (for build/test scripts)
- `Make`                   (for build scripts)
- `gcc-mingw-w64-x86-64`   (for EFI application)
- `gcc`                    (for C programs on host and for OS)
- `qemu-system-x86_64`     (Virtual Machine to run OS)
- `Rufus`                  (To flash USB with OS, Windows)
- `dd`                     (To flash USB with OS, Linux)
- `mkgpt`                  (Build image with GUID Partition Table, you have to clone and compile it yourself, easy install WIP)



# Installing (INCOMPLETE)

## Linux
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


## NixOS
WIP

Useful commands to build mkgpt:
```bash
git clone https://github.com/jncronin/mkgpt
cd mkgpt
nix-shell -p automake autoconf libtool pkg-config
./configure
make
```

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


# Some output when booting

![](./docs/img/texture_render.png)

```log
Image loaded at: 0x6022000
Hello World
static_ip = 192.168.100.54
netboot_port = 2493
netboot_ips[0] = 192.168.100.50
netboot_ips[1] = 192.168.0.60
BOOT: Found ACPI 1.0, RSDP at 777e000.
BOOT: Found ACPI 2.0, XSDP at 777e014.
No response for DHCP, using static IP address: 192.168.100.54
Did not receive ARP reply. IP doesn't exist or we need to wait longer.
Loading Kernel from disk
UEFI - Exit boot services
Loaded default font
Initializing physical memory regions
MADT LAPIC address: fee00000
MADT flags: 1
LAPIC (type=0 len=8)
 acpiProcessorID: 0
 apicID: 0
 flags: 1
IOAPIC (type=1 len=12)
 ioapicID: 0
 ioapicAddress: fec00000
 globalSystemInterruptBase: 0
IOAPIC int.src.ovr. (type=2 len=10)
 busSource: 0
 irqSource: 0
 globalSystemInterrupt: 2
 flags: 0
IOAPIC int.src.ovr. (type=2 len=10)
 busSource: 0
 irqSource: 5
 globalSystemInterrupt: 5
 flags: d
IOAPIC int.src.ovr. (type=2 len=10)
 busSource: 0
 irqSource: 9
 globalSystemInterrupt: 9
 flags: d
IOAPIC int.src.ovr. (type=2 len=10)
 busSource: 0
 irqSource: 10
 globalSystemInterrupt: 10
 flags: d
IOAPIC int.src.ovr. (type=2 len=10)
 busSource: 0
 irqSource: 11
 globalSystemInterrupt: 11
 flags: d
LAPIC nonmask.int. (type=4 len=6)
 acpiProcessorID: 255
 flags: 0
 LINT: 1
HPET measured: 427886 K cycles/ms
Disk devices: 1
Mounted QEMU HARDDISK (7 MB) at /dev0p0
Searching mounted device QEMU HARDDISK (7 MB)
Found GPT Partition 'part1' (#0) at LBA 34 - 15362
Found file/directory back1.png
STBI parse
Rendered
```

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
