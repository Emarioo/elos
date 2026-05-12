#!/usr/bin/env python3

'''
This script compiles all tools/binaries by default.
For the time being we also run kernel in QEMU but we probably won't later.

The following tools/binaries exist:
- elos.img

'''

import os, sys, platform, shutil, shlex, glob, math, threading, multiprocessing, dataclasses, subprocess
from dataclasses import dataclass


########################
#      CONSTANTS
########################

ROOT = os.path.abspath(os.path.dirname(__file__))

AS = "as"
CC = "gcc"
LD = "ld"

VERBOSE = False

def main():
    global VERBOSE

    # CONFIG
    run       = False
    gdb       = False
    vbox      = False
    iso       = False
    img       = False
    install   = False
    clean     = False

    argi = 1
    while argi < len(sys.argv):
        arg = sys.argv[argi]
        argi += 1

        if arg == "-h" or arg == "--help":
            print("cat build.py")
        elif arg == "-v" or arg == "--verbose":
            VERBOSE = True
        elif arg == "run":
            run = True
        elif arg == "gdb":
            run = True
            gdb = True
        # elif arg == "vbox":
        #     run = False
            # vbox = True
        elif arg == "img":
            img = True # just produce image, no implicit run
        # elif arg == "usb":
        #     run = False
        #     usb = True
        elif arg == "clean":
            clean = True
        elif arg == "iso":
            iso = True
        elif arg == "install":
            install = True
        else:
            print(f"Unknown argument '{arg}'")
            exit(1)

    if len(sys.argv) <= 1:
        run = True

    if clean:
        if os.path.exists("bin"):
            shutil.rmtree("bin")
        if os.path.exists("int"):
            shutil.rmtree("int")
        if os.path.exists("releases"):
            shutil.rmtree("releases")
        return 0

    cmd(f"make -f {ROOT}/netboot_server/Makefile")

    if vbox:
        build_elos("bin/elos.img")
        cmd("dd if=bin/elos.img of=bin/elos_padded.img bs=1M count=64 conv=sync")
        vdi_path = "/mnt/d/vms/elos.vdi"
        cmd(f"rm -f {vdi_path}")
        cmd(f"VBoxManage convertfromraw bin/elos_padded.img {vdi_path} --format VDI")
    elif install:
        install_deps()
    # elif usb:
    #     build_elos("bin/elos")
    #     build_image("bin/elos.img")
        # cmd("dd if=bin/elos.img of=bin/elos_padded.img bs=1M count=64 conv=sync")
    else:
        package_elos("releases", iso)
        

    if run:
        # TODO: DON'T HARDCODE PATHS
        OVMF_FD = "extern/ovmf/OVMF.fd"

        qemu_flags = f'''
            -bios {OVMF_FD}
            -netdev tap,id=net0,ifname=tap0,script=no,downscript=no
            #-netdev user,id=net0
            -device e1000,netdev=net0  # e1000 ~= intel 8254x
            -machine pc                # PS/2 keyboard input
            -serial file:kernel.log 
            -s 
            {"-S " if gdb else ""}

            # -device ahci,id=ahci
            # -device ide-drive,drive=disk0,bus=ahci.0
            # -nographic
        '''
        # @NOTE Not sure what these flags do but seems useful/important
        # Use -L if you want whole firmware package (seems to use secure boot with test keys)
        # f"-L /usr/share/ovmf/ "
        # f"-drive format=raw,file=bin/OVMF.fd,if=pflash "   # -pflash (but without warnings)

        if not iso:
            qemu_flags += f'''
                -drive format=raw,file=bin/elos.img,if=ide,media=disk,cache=writeback
                # -drive if=none,id=disk0,format=raw,file=bin/elos.img
            '''
        else:
            # If you want to use ISO
            qemu_flags += f'''
            -cdrom bin/elos.iso
            '''

        qemu_flag_list = qemu_flags.splitlines()
        qemu_flags = ""
        for line in qemu_flag_list:
            at = line.find("#")
            if at == -1:
                flag = line.strip()
            else:
                flag = line[:at].strip()
            if len(flag) > 0:
                print(line, "->", flag)
                qemu_flags += flag + " "

        cmd(f"qemu-system-x86_64 {qemu_flags}")


def package_elos(release_dir, build_iso = False):
    name    = "elos"
    version = "0.1.0"
    arch    = "x86_64"

    temp_folder_name = f"{name}-{version}-{arch}"
    temp_folder_path = f"{release_dir}/{temp_folder_name}"

    os.makedirs(temp_folder_path, exist_ok=True)

    os.makedirs(temp_folder_path+"/fs", exist_ok=True)

    os.makedirs(temp_folder_path+"/fs/EFI/BOOT", exist_ok=True)

    iso_path     = f"{temp_folder_path}/elos.iso"
    img_path     = f"{temp_folder_path}/elos.img"
    kernel_elf_path     = f"{temp_folder_path}/kernel.elf"
    bootx64_path = f"{temp_folder_path}/fs/EFI/BOOT/BOOTX64.EFI"
    kernel_path  = f"{temp_folder_path}/fs/kernel.img"

    INT_DIR=f"{ROOT}/int"

    cmd(f"make -f {ROOT}/boot/Makefile INT_DIR={INT_DIR}/boot BOOT_EFI={bootx64_path}")
    
    cmd(f"make -f {ROOT}/kernel/Makefile INT_DIR={INT_DIR}/kernel KERNEL_IMAGE={kernel_path} KERNEL_ELF={kernel_elf_path}")

    fat_path = f"{INT_DIR}/fat.img"

    
    DEPS_SPEC: list[tuple[str,str]] = [
        (bootx64_path, "EFI/BOOT/BOOTX64.EFI"),
        (kernel_path, "kernel.img"),
        ("res/Lat2-Terminus16.psf", "RES/STDFONT.PSF"),
    ]

    ISO_DIR = f"{INT_DIR}/image"
    fat_size = make_fat(fat_path, DEPS_SPEC)

    cmd(f"cp {fat_path} {ISO_DIR}/fat.img")
    #                       GPT header info      fat    some extra rom
    gpt_size_estimation = 2 * (2*512 + 128*128) + fat_size + (40 + 400) * 512
    cmd(f"mkgpt -o {img_path} --image-size {gpt_size_estimation/512} --part {fat_path} --type system")

    # subprocess.Popen(
    if build_iso:
        cmd(f"xorriso -as mkisofs -R -f -e fat.img -no-emul-boot -o {iso_path} {ISO_DIR}")
    # cmd(f"xorriso -as mkisofs -R -f -no-emul-boot -o {iso_path} {ISO_DIR}")

    # Copy ISO into folder

    # Copy raw image into folder

    # Zip folder

    cmd(f"cd {os.path.dirname(temp_folder_path)} && tar -czf {temp_folder_name}.tar.gz {temp_folder_name}")

    os.makedirs("bin", exist_ok=True)

    # Copy latest images to bin for quick access (we could make symlinks)
    if os.path.exists(img_path):
        cmd(f"cp {img_path} bin/elos.img")
    if os.path.exists(bootx64_path):
        cmd(f"cp {bootx64_path} bin/boot.elf")
    if os.path.exists(kernel_elf_path):
        cmd(f"cp {kernel_elf_path} bin/kernel.elf")
        cmd(f"objdump -Sr bin/kernel.elf > bin/kernel.dis")
    if os.path.exists(iso_path):
        cmd(f"cp {iso_path} bin/elos.iso")

    print(f"Successfully built \033[32m{temp_folder_path}\033[0m")

# Returns FAT size
def make_fat(out_path: str, deps_spec: list[tuple[str,str]]):

    INT_DIR = "int/image"
    
    # Collect dependencies
    DEPS = []
    for d in deps_spec:
        if d[0].find("*") == -1:
            # File
            # os.makedirs(os.path.dirname(d[1]), exist_ok=True)
            DEPS.append(d)
        else:
            # Wild card directory
            for f in glob.glob(d[0], recursive=True):
                outf = os.path.join(d[1], os.path.basename(f))
                # os.makedirs(d[1], exist_ok=True)
                DEPS.append((f, outf))

    # Compute file size
    totalSize = 0
    for src, dst in DEPS:
        s = os.path.getsize(src)
        totalSize += s
        # print(f"{src:15} {math.ceil(s/1024)}KB")

    fatSize = math.ceil(math.ceil(totalSize * 1.25) / 512) * 512
    
    # I don't remember why I chose 4200. Possible reaons:
    #   1. 4096 * 1024 is minimum for FAT that UEFI likes.
    #         Don't think this is true, 4096 sectors might be true.
    #   2. 4200 * 1024 means 8K sectors so FAT16 will be used.

    # min_fat_size = 32*1024*1024 # Virtual Box doesn't seem to like EFI System Partitions smaller than 32 MiB (wiki.osdev.org/UEFI_App_Bare_Bones)
    min_fat_size = 4200*512*2

    if fatSize < min_fat_size:
        fatSize = min_fat_size

    # print(totalSize, fatSize)

    if os.path.exists(out_path):
        os.remove(out_path)

    cmd(f"dd if=/dev/zero of={out_path} bs=1k count={math.ceil(fatSize/1024)} conv=fsync")
    cmd(f"mformat -i {out_path} ::")

    # Copy files
    for src, dst in DEPS:
        assert dst[0] != '/', f"{src} -> {dst}"
        
        split = os.path.dirname(dst).split("/")
        acc = ""
        if len(split) > 1 or len(split[0]) > 0:
            for s in split:
                acc = os.path.join(acc, s)
                cmd(f"mmd -D o -i {out_path} ::/{acc}")
        cmd(f"mcopy -D o -i {out_path} {src} ::/{dst}")

        int_dst = os.path.join(INT_DIR, dst)
        os.makedirs(os.path.dirname(int_dst), exist_ok=True)
        shutil.copy(src, int_dst)

    return fatSize


def install_deps():
    global VERBOSE
    VERBOSE = True # print commands that we run

    if platform.system() == "Windows":
        print("You have to install dependencies manually on Windows.")
        print("Sorry.")
    elif platform.system() == "Linux":
        cmd("sudo apt install gcc")
        cmd("sudo apt install gcc-mingw-w64-x86-64")
        cmd("sudo apt install qemu-system-x86")
        cmd("sudo apt install xorriso")
    else:
        print(f"Platform {platform.system()} not supported by 'build.py install'")
        print(f"You'll have to install dependencies manually ):")

def cmd(c):
    if platform.system() == "Windows":
        c = c.replace('/', "\\")
        sp = c.split(" ")
        sp[0] += ".exe"
        c = " ".join(sp)
    
    if VERBOSE:
        print(c, file=sys.stderr)
    err = os.system(c)
    if err:
        if not VERBOSE:
            print("ERR",c)
        exit(1)

    return 0

if __name__ == "__main__":
    main()