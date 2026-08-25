
Problems to solve

# Multiple people on one computer

Two people typing on their keyboard on two monitors on one computer.

The application must now which keyboard to detect keys from.

More than one application in focus at any one time.

# Kernel modules

A kernel module is a piece of code that runs in the kernel. It can call kernel functions and write to hardware memory.
You can load and unload kernel modules while the system is running.

There are a couple of use cases for this:
- You can add support for hardware the OS doesn't normally support.
- Faster development cycle where you can implement a driver for network/usb/disk while the system is running.


**What kind of security/safety do we expect?**
A malicious kernel module will mess up your system real bad. But we do expect the kernel modules to have bugs and some flaws
which we can try to handle nicely when possible. CPU exceptions for example.

On CPU expections kernel will call `error_handler` where module can try to shutdown any hardware or report useful information. Most notably disable audio device so it does not repeat the same sample buffer.
Kernel does not expect module to recover from any faults and will shutdown the module when it exits the handler, reaches a timeout, or gets a second fault.

**What API does kernel module access?**
Option 1. Same functions and memory as the kernel. No special API.

Kernel module has full access to everything which means it can mess with core structures and page tables.
But we don't need to maintain an API specific for kernel modules. On the other hand we could provide the same
API as the components we have separated. 'CPU_enable_interrupts' and 'CPU_core_index' and 'NET_scan_devices' are directly available for example and
written and expected to be called from kernel internals but also kernel modules.

Option 2. Special API for the module. PCI and USB functions for example since it may require some synchronization between multiple modules so they don't write
to the same PCI device. This could go into option one where we provide USB and PCI components which we don't have currently.

