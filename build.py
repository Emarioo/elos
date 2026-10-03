#!/usr/bin/env python3

'''
This script compiles all tools/binaries by default.
For the time being we also run kernel in QEMU but we probably won't later.

The following tools/binaries exist:
- elos.img

'''

import os, sys, math, re, platform, shutil, shlex, glob, math, threading, multiprocessing, dataclasses, subprocess
from dataclasses import dataclass

from typing import Callable

from scripts import tools
from scripts.tools import cmd_async, wait_pool, cmd, cmd_back

########################
#      CONSTANTS
########################

ROOT = os.path.abspath(os.path.dirname(__file__))

AS = "as"
CC = "gcc"
LD = "ld"

MKGPT = f"{ROOT}/../mkgpt/mkgpt" if os.path.exists(f"{ROOT}/../mkgpt/mkgpt") else "mkgpt"

# Tap is created with 'sudo scripts/maketap.sh'
HAS_TAP = os.path.exists("/sys/class/net/tap0")

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
    netboot   = False

    second_qemu = False

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
            img = True
        elif arg == "gdb":
            img = True
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
            img = True
            iso = True
        elif arg == "install":
            install = True
        elif arg == "netboot":
            netboot = True
        elif arg == "second":
            second_qemu = True
        else:
            print(f"Unknown argument '{arg}'")
            exit(1)

    if len(sys.argv) <= 1:
        run = True
        img = True

    if install:
        install_deps()
        exit(0)

    if clean:
        if os.path.exists("bin"):
            shutil.rmtree("bin")
        if os.path.exists("int"):
            shutil.rmtree("int")
        if os.path.exists("releases"):
            shutil.rmtree("releases")
        return 0
    
    os.makedirs("bin", exist_ok=True)

    netboot_server = "int/netboot_server_windows/netboot.exe" if platform.system() == "Windows" else "int/netboot_server_linux/netboot"
    netboot_server_bin = f"bin/{os.path.basename(netboot_server)}"
    
    def sync0():
        cmd(f"make -f {ROOT}/netboot_server/Makefile")
        if os.path.exists(netboot_server):
            try:
                shutil.copy(netboot_server, netboot_server_bin)
            except:
                pass
            
    net_thread = cmd_async(sync0)

    if vbox:
        print("VBOX NO WORK")
        exit(1)
        # build_elos("bin/elos.img")
        # cmd("dd if=bin/elos.img of=bin/elos_padded.img bs=1M count=64 conv=sync")
        # vdi_path = "/mnt/d/vms/elos.vdi"
        # cmd(f"rm -f {vdi_path}")
        # cmd(f"VBoxManage convertfromraw bin/elos_padded.img {vdi_path} --format VDI")
    # elif usb:
    #     build_elos("bin/elos")
    #     build_image("bin/elos.img")
        # cmd("dd if=bin/elos.img of=bin/elos_padded.img bs=1M count=64 conv=sync")


    if img:
        release_dir = "releases"
        package = tools.default_package(release_dir)
        tools.package_elos(package, iso)


    wait_pool([net_thread])
    if netboot:
        cmd(f"{netboot_server_bin}")

    elif run or second_qemu:
        # TODO: DON'T HARDCODE PATHS
        OVMF_FD = "extern/ovmf/OVMF.fd"

        DISK_IMG = "int/disk.img"
        DISK_NVME_IMG = "int/disk_nvme.img"
        # # if not os.path.exists(DISK_IMG):

        HAS_NVME = True

        log_file   = "bin/kernel.log"
        img_file   = "bin/elos.img"
        tap_device = "tap0"
        net_name   = "net0"
        mac = "52:54:00:12:34:56"
        if second_qemu:
            tap_device = "tap1"
            new_img_file = "bin/elos1.img"
            shutil.copy(img_file, new_img_file)
            img_file = new_img_file
            log_file = "bin/kernel1.log"
            net_name   = "net1"
            mac = "10:20:30:30:20:10"
            
        # DEPS_SPEC: list[tuple[str,str]] = [
        #     ("scripts/disk_fs/*", ""),
        # ]
        # make_gpt(DISK_IMG, DEPS_SPEC)

        if HAS_NVME and not os.path.exists(DISK_NVME_IMG):
            cmd(f"qemu-img create -f raw {DISK_NVME_IMG} 64M")
            # cmd(f"gcc scripts/fwrite.c -g -o int/fwrite && int/fwrite {DISK_IMG}")
        
        HAS_AUDIO = False
        # HAS_AUDIO = True

        core_count = 2
        qemu_flags = f'''
            -enable-kvm -cpu host
            -bios {OVMF_FD}
            -machine pc                # PS/2 keyboard input
            -serial file:{log_file}
            {"-s -S " if gdb else ""}
            -smp {core_count}

            -device ahci,id=ahci

            # -M q35

            # -trace "pci_cfg_write"
            # -drive  file={DISK_IMG},if=none,id=disk1,format=raw
            # -device ide-hd,drive=disk1,bus=ahci.1
            # -nographic
        '''
        if HAS_NVME:
            qemu_flags += f'''
            -drive  file={DISK_NVME_IMG},if=none,id=nvm1,format=raw
            -device nvme,serial=deadbeef,drive=nvm1
            '''
        if HAS_AUDIO:
            qemu_flags += f'''
            -device intel-hda #,debug=4
            -device hda-output
            # -device hda-duplex # includes microphone
            '''
        if HAS_TAP:
            qemu_flags += f'''
            -device e1000e,netdev={net_name},mac={mac}  # e1000 ~= intel 8254x, e1000e ~= intel 82574L
            -netdev tap,id={net_name},ifname={tap_device},script=no,downscript=no
            #-netdev user,id={net_name}
            '''

        # @NOTE Not sure what these flags do but seems useful/important
        # Use -L if you want whole firmware package (seems to use secure boot with test keys)
        # f"-L /usr/share/ovmf/ "
        # f"-drive format=raw,file=bin/OVMF.fd,if=pflash "   # -pflash (but without warnings)

        if not iso:
            qemu_flags += f'''
                -drive file={img_file},if=none,id=disk0,format=raw
                -device ide-hd,drive=disk0,bus=ahci.0
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
                qemu_flags += flag + " "

        cmd(f"qemu-system-x86_64 {qemu_flags}")

        parse_fault()


def parse_fault():
    path = "bin/kernel.log"

    with open(path, "r") as f:
        text = f.read()

    m = re.search(r'rip=0x([0-9a-f]+)', text, re.IGNORECASE)
    if m is not None and len(m.groups()) > 0:
        rip_addr = int(m.groups()[0], base = 16)

        stride = 0x1000000
        app_index = math.floor((rip_addr - 0xC0000000) / stride)
        apps = [
            "prism.dis",
            "doom.dis",
            # "slate.dis",
        ]

        if app_index >= 0 and app_index < len(apps):
            base = 0xC0000000 + app_index * stride
            app_dis = apps[app_index]
        else:
            base = 0
            app_dis = "bin/kernel.dis"

        with open(app_dis, "r") as f:
            disas = f.read()

        word = hex(rip_addr - base)[2:]

        print(f"rip={hex(rip_addr)}  {word} {app_dis}")
        # ser = re.search(word, disas)
        at = disas.find(word)
        if at != -1:
            linenr = disas.count('\n', 0, at)
            location = f"{app_dis}:{linenr}"
            print(location)
            cmd(f"echo {location} >> {path}")


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


if __name__ == "__main__":
    main()