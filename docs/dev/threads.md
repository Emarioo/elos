
How to support something like this?

You have work you want to do in parallel.
How to do we make it easy in C?

```python

def sync0():
    cmd(f"make -f {ROOT}/netboot_server/Makefile")
    if os.path.exists(netboot_server):
        try:
            shutil.copy(netboot_server, netboot_server_bin)
        except:
            pass
        
cmd_async(sync0)


def work0():
    cmd(f"make -f {ROOT}/boot/Makefile INT_DIR={INT_DIR}/boot BOOT_EFI={bootx64_path}")

def work1():
    cmd(f"make -f {ROOT}/kernel/Makefile INT_DIR={INT_DIR}/kernel KERNEL_IMAGE={kernel_path} KERNEL_ELF={kernel_elf_path}")

def work2():
    import apps.prism.build
    apps.prism.build.main(prism_path)
    cmd(f"objdump -S {prism_path} > bin/prism.dis")
    
def work3():
    import apps.terminal.build
    apps.terminal.build.main(term_path)
    cmd(f"objdump -S {term_path} > bin/term.dis")

threads = []
threads.append(cmd_async(work0))
threads.append(cmd_async(work1))
threads.append(cmd_async(work2))
threads.append(cmd_async(work3))

wait_pool(threads)
    


```
