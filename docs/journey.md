
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

----------------------------

I want to experiment. Create tools, have fun, learn.

What do I know I want in my OS.
What we always have is hardware.
- A processor with multiple threads running at some speed with some architecture with various extensions.
- A flat volatile physical memory space
- A permanent storage space
- Hot pluggable devices like keyboards, mouse, USB drive/storage
- Network device
- Graphics Card

In our computer software universe we got these limits 16GB RAM, 256GB storage, 4Ghz 8 core CPU and
we want our OS to allow us to utilize this for some purpose.

One purpose is running application to perform computations, logging, database storage, server requests.
Play games. Watch videos, movies (altough that's far far away)

If the kernel is the manager of the hardware providing interface to OS and
the OS provides interface for user how would I like to experiment with the kernel architecture.

# Let's begin with permanent storage
You want to store data. Either applications, games, compute software, text editor application.
Maybe just simple text, like todo notes. Or a server request log.

How would we like to store these things?

What we have on the hardware side is storage device with some number of sectors.
Usually formatted with MBR or GPT.

Kernel accesses it directly. OS can access device through kernel API

kernel__Function_API

Kernel_API

 format where our kernel
can access partitions. But on embedded for example, we may just have a file system flat on the
storage device, no partitions. If we're running embedded would we really have this kernel and OS though.
Well no it's rare but where do we put the limit.

I suppose we just want the kernel to provide easy of use functions.
Either write to a storage device directly. Or 

We want our OS and kernel to be living in harmony with other OSes on the same computer.
So a partition with some allocated space is the fundamental primitive we work with.
On the OS side do we want to abstract this? We'll we would like access to it and be able
to access raw partitions and the storage device directly but that is not the default.
Kernel and OS needs to simplify this.

Once again, where do we draw the line. Well if we want a user application (privlaged of course)
to access partitions then our Kernel needs to provide this ability.

