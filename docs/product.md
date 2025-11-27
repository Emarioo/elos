
At the end of the day we produce an `elos.iso` or `elos.img` compatible with UEFI.

The .iso is understood by Rufus and other programs which you can use to
blit the contents (OS) to a USB drive.

The .img is a raw image file containing a GUID Partition Table and one partition with
FAT32 file system with EFI application and other OS content. (.iso also has EFI)

What we produce in raw form for both formats is a directory with
- `/EFI/BIOS/BOOTX64.EFI`, UEFI application, loads OS
- `/ELOS/<content>`, Kernel and OS


# Emulation

If the OS and some user applications could be emulated on
Windows, it would be great for iteration.
Testing user programs and logic for example.

We abstract the core API of the kernel, device drivers.
Apps, scheduling we keep. Altough we can't schedule proceses


# For Legacy BIOS
(my thoughts doesn't work)

The image looks like this.

For some context, the image is blitted to a USB drive.
The drive contains sectors, we assume 512 bytes each.

We have a Master Boot Record in the first sector.
It has a partion table with one partition with FAT16 file format.

|Physical sector|Section|
|-|-|
|0|Master Boot Record|
|1|Volume Boot record (start of partition #1)|
|2-10|File allocation table 2|
|11-19|File allocation table 2|
|20-33|Root directory|
|34-N|File data (clusters)|
