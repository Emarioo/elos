

# When booting from USB
```
/boot   # mounted to USB FAT32 EFI system partition
/       # mounted to RAM DISK which is a copy of INITRD, INITRD is untouched on writes.

/boot/EFI/BOOT/BOOTX64.EFI
/boot/INITRD.IMG
/boot/KERNEL.IMG

/elos/system.cfg
/pkg/prism/prism.elf
/pkg/slate/slate.elf
/pkg/term/term.elf
```


# When booting from SSD where ELOS was installed
```
/boot   # mounted to SSD FAT32 EFI system partition
/       # mounted to SSD Normal partition


```



You may later add `/mydata` and mount it to a second SSD if you have one.

# File system


```
/boot/EFI/BOOT/BOOTX64.EFI
/boot/KERNEL.IMG

/home/user
/elos/system.cfg
/pkg

/pkg/prism/prism.elf
/pkg/slate/slate.elf
/pkg/term/term.elf

```

Later we may add `/tmp` which is mounted to a RAM disk which is cleansed on reboot.
By convention external disks are mounted at `/media`.

## Thoughs


     /home/user 
     /boot -> fat32 EFI system partition, ex /boot/EFI/BOOT/BOOTX64.EFI
     /elos/boot        Maybe this is better to indicate it is elos boot files specifically. Still fat32 EFI system partition.
                       When we haven't installed the OS on an SSD does it refer to the USB boot files?
     /elos/pkg         Similar to /nix/store?
                       Do we put text editor, terminal, prism compositor here?
                       If we haven't installed OS on SSD can we even do this?
                       It's not mounted yet? Mount on RAM FS maybe?
     /elos/kernel      Kernel files? do we need any or is the boot kernel files enough?
                       If we haven't installed on an SSD do we even have this?
     /elos/??? anything else?

        When we boot we start from a USB, FAT32 EFI SYSTEM PARTITION.

        It is now We want to install it on our permanent SSD.

        Next time we start it will boot from that SSD.
        The FAT32 system partition on the USB still needs to exist to boot.
        It must be moved to the SSD. We can tweak it a bit as necessary.
        For example when booting from USB we may have boot_config=true in a text file
        so you can select OS installation method and which disc/partition in particular (grub or no grub for example).
        When moving boot files to SSD we may have boot_config=false instead so we boot right into
        desktop interface.

        We have two disk devices: USB with FAT32 and SSD with FAT32 and ext2 (or whatever FS we choose)
        USB is later removed.

        We refer to data on disc using paths. Disks and partitions must be mounted upon the Virtual File System which
        handle path resolution. Windows uses volumes we do not. We do it like linux.

        Disks usually have a partition system. With special care it allows dual booting operating systems.
        When mounting it is not enough to refer to just a disk drive, the partition index or GUID is needed as well.

        Where is data stored in the VFS. For example boot files?
