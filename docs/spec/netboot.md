**NetBoot** is a protocol for downloading files from the network and booting.

The bootloader (EFI application) downloads the Kernel/OS files, loads them into memory or on the disk and then enters the Kernel.
An EFI application should fallback to booting from disk if **NetBoot Server** isn't running.

Having **NetBoot** in ELOS does not mean a smaller ELOS image. The same kernel and startup files are still there and can be booted from without network.
If NetBoot can reach a NetBoot Server then it will try to download instead of reading files from disk.


The **NetBoot Protocol** is just a File Transfer Protocol with some extra metadata such as which file to load into memory right away kernel.img and a Message Type to list all
downloadble kernel files on the server. It also provides timestamps (or checksums?) per file where client doesn't request files it already has.

(listing files and timestamps are not implemented yet)

# How to setup

The first thing to do is to modify the IP address(es) to the NetBoot server in [boot/template.cfg](../../boot/template.cfg).
Find the IP address to your machine with `ipconfig` on windows, `ip addr` on linux.

In QEMU on Linux or WSL you will need to setup tap0 device which can be quirky. Booting from network isn't necessary in QEMU since you can just rebuild the kernel and restart QEMU.
On a computer you have to move around USB drive when recompiling kernel. Booting from network is great on hardware because we don't have to move around USB drive (unless you recompile the EFI application).

Now you can build the Operating System.
```bash
build.py iso
```

Now in release directory you will have `elos.iso` which you flash onto a USB drive. Rufus on Windows, dd or something on linux? Maybe flash `elos.img` which is GPT image on Linux?

Put the USB Drive in your computer and boot into BIOS (spam Delete or F2 when powering on).
Select the USB Drive and boot. You should see Operating System running if it is compatible with your computer.

Earlier step was just to check that you can run OS to begin with. Now it's time to build and launch NetBoot Server.
```bash
build.py netboot
```

Now restart your computer (may have to go into BIOS and select USB drive again). If NetBoot server terminal or OS tells you "Requesting kernel.img" then it works.
If you get DHCP or ARP problems then OS may not support your network controller.


