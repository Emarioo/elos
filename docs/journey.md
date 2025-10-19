
QEMU is more accepting of the bootable medium. RAW binary with boot sector is enough.

Booting in VirtualBox needs either an ISO file or that you create a hard disk from
the image. VirtualBox won't accept small binaries so pad with zeros to 1 MB.

`dd if=elos.img of=elos_padded.img bs=1M count=64 conv=sync`
`VBoxManage.exe convertfromraw elos_padded.img D:\vms\elos.vdi --format VDI`

Booting on a modern motherboard seems to require UEFI. There is CSM option in BIOS but
it didn't help on my motherboard. Maybe the `dd` command i used didn't format the USB drive correctly.
The USB is recognized as FAT12/16 file system on Windows and Linux so it seems like UEFI is the step to take
if I want this working. Maybe BIOS would like USB with ISO format?

`sudo dd if=os.img of=/dev/sda1 bs=512 conv=fsync`

Okay so we don't need ISO. I just format a USB drive with my raw image. We can make an .ISO for my kernel later.
In WSL you can use this: https://github.com/dorssel/usbipd-win/releases/tag/v5.3.0.
Then access USB devices so I don't have to dual boot a Linux distro or develop on Linux.

```
# In windows
usbipd list
usbipd bind --busid 4-6
usbipd attach --wsl --busid 4-6

# On Linux
lsusb
lsblk

# In windows
usbipd detach --busid 4-6
```
