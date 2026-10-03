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



########################
#      CONSTANTS
########################

ROOT = os.path.abspath(os.path.dirname(os.path.dirname(__file__)))

AS = "as"
CC = "gcc"
LD = "ld"

MKGPT = f"{ROOT}/../mkgpt/mkgpt" if os.path.exists(f"{ROOT}/../mkgpt/mkgpt") else "mkgpt"

VERBOSE = False

@dataclass
class PackageItem:
    source_path: str      = None # Path on the host OS
    dst_path:    str      = None # Path to move source_path to in initrd
    func:        Callable = None # func to create the source path (if executable that needs compiling for example)

@dataclass
class PackageOS:
    name:    str
    version: str
    arch:    str

    temp_folder_name: str = None
    temp_folder_path: str = None
    items: list[PackageItem] = dataclasses.field(default_factory=list)
    auto_run_paths: list[str] = dataclasses.field(default_factory=list)

@dataclass
class RunConfig:
    img_path: str
    log_path: str
    
    mac: str = "52:54:00:12:34:56"
    core_count: int = 2

    timeout_sec: float = 5.0
    auto_reboot: bool = False


def default_package(release_dir: str) -> PackageOS:
    
    APP_DIR = f"{ROOT}/apps"
    
    DOOM_MAKEFILE   = f"{ROOT}/../doomgeneric/doomgeneric/Makefile.elos"
    DOOM_WAD        = f"{ROOT}/../iwad/doom1.wad"

    # @TODO Provide copyright-free sound for testing.
    WAV_PATH        = f"{ROOT}/dream_aria.wav"

    package = PackageOS(
        name    = "elos",
        version = "0.0.1",
        arch    = "x86_64",
    )

    temp_folder_name = f"{package.name}-{package.version}-{package.arch}"
    temp_folder_path = f"{release_dir}/{temp_folder_name}"
    package.temp_folder_name = temp_folder_name
    package.temp_folder_path = temp_folder_path
    
    doom_path       = f"{temp_folder_path}/initrd/pkg/doom/doom.elf"
    wad_path        = f"{temp_folder_path}/initrd/pkg/doom/doom1.wad"
    wintest         = f"{temp_folder_path}/initrd/pkg/win32_loader/wintest.exe"
    
    def win32_func():
        cmd(f"APP_OUTPUT2={wintest} make -f apps/win32_loader/Makefile wintest")
        cmd_back(f"objdump -S {wintest} > wintest.dis")
        
    def doom_func():
        cmd(f"OUTPUT={doom_path} make -f {DOOM_MAKEFILE}")
        cmd_back(f"objdump -S {doom_path} > doom.dis")

    package.items = [
            PackageItem(f"{APP_DIR}/prism"),
            PackageItem(f"{APP_DIR}/terminal", "PKG/TERM/TERM.ELF"),
            PackageItem(f"{APP_DIR}/slate"),
            PackageItem(f"{ROOT}/res/Lat2-Terminus16.psf", "PKG/SLATE/STDFONT.PSF"),
            PackageItem(f"{APP_DIR}/supper"),
            PackageItem(f"{APP_DIR}/netchat"),

            PackageItem(f"{APP_DIR}/win32_loader"),
            PackageItem(wintest, "PKG/win32_loader/wintest.exe", func = win32_func),

            PackageItem(f"{ROOT}/boot/template.cfg", "TEMPLATE.CFG"),
    ]

    provide_doom = os.path.exists(DOOM_MAKEFILE) and os.path.exists(DOOM_WAD)
    if provide_doom:
        os.makedirs(os.path.dirname(wad_path), exist_ok=True)
        cmd(f"cp {DOOM_WAD} {wad_path}")
        package.items.append(PackageItem(doom_path, "PKG/DOOM/DOOM.ELF", func = doom_func))
        package.items.append(PackageItem(wad_path, "PKG/DOOM/DOOM1.WAD"))
    else:
        print(DOOM_MAKEFILE, DOOM_WAD)

    if os.path.exists(WAV_PATH):
        package.items.append(PackageItem(WAV_PATH, "PKG/WAV/DREAM.WAV"))
    else:
        print(f"\033[33mWARNING:\033[0m Could not find {WAV_PATH} (it won't be included in PKG)")

    return package

def package_elos(package: PackageOS, build_iso = False):

    temp_folder_name = package.temp_folder_name
    temp_folder_path = package.temp_folder_path

    os.makedirs(temp_folder_path, exist_ok=True)
    os.makedirs(temp_folder_path+"/fs", exist_ok=True)
    os.makedirs(temp_folder_path+"/initrd", exist_ok=True)
    os.makedirs(temp_folder_path+"/fs/EFI/BOOT", exist_ok=True)

    iso_path        = f"{temp_folder_path}/elos.iso"
    img_path        = f"{temp_folder_path}/elos.img"
    kernel_elf_path = f"{temp_folder_path}/kernel.elf"
    bootx64_path    = f"{temp_folder_path}/fs/EFI/BOOT/BOOTX64.EFI"
    kernel_path     = f"{temp_folder_path}/fs/KERNEL.IMG"
    initrd_path     = f"{temp_folder_path}/fs/INITRD.IMG"

    INT_DIR         = f"{ROOT}/int"
    fat_path        = f"{INT_DIR}/fat.img"


    threads = []
    
    # cmd(f"make -f apps/libm/Makefile")

    def sync0():
        cmd(f"make -f {ROOT}/boot/Makefile INT_DIR={INT_DIR}/boot BOOT_EFI={bootx64_path}")
    
    def sync1():
        cmd(f"make -f {ROOT}/kernel/Makefile INT_DIR={INT_DIR}/kernel KERNEL_IMAGE={kernel_path} KERNEL_ELF={kernel_elf_path}")
    
    threads.append(cmd_async(sync1))
    
    wait_pool(threads)

    threads.append(cmd_async(sync0))

    auto_run_text = "\n".join(package.auto_run_paths) + "\n"
    auto_run_file = f"{INT_DIR}/autorun.txt"
    os.makedirs(os.path.dirname(auto_run_file), exist_ok=True)
    with open(auto_run_file, "w") as f:
        f.write(auto_run_text)
    package.items.append(PackageItem(auto_run_file, "sys/autorun.txt"))

    wait_pool(threads)

    
    threads = []
    DEPS_SPEC: list[tuple[str,str]] = [ ]
    
    def app_sync(makefile, app_path):
        # print(makefile, app_path)
        basename = os.path.basename(app_path)
        cmd(f"APP_OUTPUT={app_path} make -f {makefile}")
        cmd_back(f"objdump -S {app_path} > {basename}.dis")

    for item in package.items:
        if item.func is not None:
            assert item.dst_path is not None, item
            assert item.source_path is not None, item
            threads.append(cmd_async(item.func))
            DEPS_SPEC.append((item.source_path, item.dst_path))
        elif os.path.exists(item.source_path + "/Makefile"):
            makefile = item.source_path + "/Makefile"
            basename = os.path.basename(item.source_path)
            dst_path = f"PKG/{basename}/{basename}.elf" if item.dst_path is None else item.dst_path
            src_path = f"{temp_folder_path}/initrd/pkg/{basename}/{basename}.elf"
            
            threads.append(cmd_async(app_sync, makefile, src_path))
            DEPS_SPEC.append((src_path, dst_path))
        else:
            assert item.dst_path is not None, item
            assert item.source_path is not None, item
            DEPS_SPEC.append((item.source_path, item.dst_path))
      
    wait_pool(threads)
        
    make_gpt(initrd_path, DEPS_SPEC)
    
    DEPS_SPEC: list[tuple[str,str]] = [
        (bootx64_path, "EFI/BOOT/BOOTX64.EFI"),
        (kernel_path, "KERNEL.IMG"),
        (initrd_path, "INITRD.IMG"),
        ("boot/template.cfg", "TEMPLATE.CFG"),
    ]

    fat_size, ISO_DIR = make_fat(fat_path, DEPS_SPEC)

    
        
    if build_iso:
        def sync1():
            cmd(f"cp {fat_path} {ISO_DIR}/fat.img")
            cmd(f"xorriso -as mkisofs -R -f -e fat.img -no-emul-boot -o {iso_path} {ISO_DIR}")
            if os.path.exists(iso_path):
                cmd(f"cp {iso_path} bin/elos.iso")
            
            # @TODO Remove this.
            #   Currently moves iso to windows for rufus.
            host_path = "/mnt/e/dev/elos/bin/elos.iso"
            if os.path.exists(os.path.dirname(host_path)):
                cmd(f"cp {iso_path} {host_path}")
                
            fs_path = f"/mnt/e/dev/elos/releases/{temp_folder_name}";
            if os.path.exists("/mnt/e/dev/elos"):
                cmd(f"cp -r releases/{temp_folder_name}/fs {fs_path}")

        threads.append(cmd_async(sync1))

    #                       GPT header info      fat    some extra rom
    gpt_size_estimation = 2 * (2*512 + 128*128) + fat_size + (40 + 400) * 512
    cmd(f"{MKGPT} -o {img_path} --image-size {gpt_size_estimation/512} --part {fat_path} --type system")

    # cmd(f"xorriso -as mkisofs -R -f -no-emul-boot -o {iso_path} {ISO_DIR}")

    # Copy ISO into folder

    # Copy raw image into folder

    # Zip folder

    def sync3():
        cmd(f"tar -C {os.path.dirname(temp_folder_path)} -czf {temp_folder_path}.tar.gz {temp_folder_name}")

    wait_pool(threads)
    
    threads.append(cmd_async(sync3))

    # Copy latest images to bin for quick access (we could make symlinks)
    if os.path.exists(img_path):
        cmd(f"cp {img_path} bin/elos.img")
    if os.path.exists(bootx64_path):
        cmd(f"cp {bootx64_path} bin/boot.elf")
    if os.path.exists(kernel_elf_path):
        cmd(f"cp {kernel_elf_path} bin/kernel.elf")
        cmd_back(f"objdump -Sr bin/kernel.elf > bin/kernel.dis")

    wait_pool(threads)

    print(f"Successfully built \033[32m{temp_folder_path}\033[0m")

def run_package(runConfig: RunConfig):
    gdb = False
    second_qemu = False
    HAS_TAP = False
    iso = False
    
    # TODO: DON'T HARDCODE PATHS
    OVMF_FD = "extern/ovmf/OVMF.fd"

    DISK_IMG = "int/disk.img"
    DISK_NVME_IMG = "int/disk_nvme.img"
    # # if not os.path.exists(DISK_IMG):

    HAS_NVME = True


    log_file   = runConfig.log_path
    img_file   = runConfig.img_path
    tap_device = "tap0"
    net_name   = "net0"
    mac = runConfig.mac
    if second_qemu:
        tap_device = "tap1"
        new_img_file = "bin/elos1.img"
        shutil.copy(img_file, new_img_file)
        img_file = new_img_file
        log_file = "bin/kernel1.log"
        net_name   = "net1"
        mac = "10:20:30:30:20:10"
        
    os.makedirs(os.path.dirname(log_file), exist_ok=True)

    # DEPS_SPEC: list[tuple[str,str]] = [
    #     ("scripts/disk_fs/*", ""),
    # ]
    # make_gpt(DISK_IMG, DEPS_SPEC)

    if HAS_NVME and not os.path.exists(DISK_NVME_IMG):
        cmd(f"qemu-img create -f raw {DISK_NVME_IMG} 64M")
        # cmd(f"gcc scripts/fwrite.c -g -o int/fwrite && int/fwrite {DISK_IMG}")
    
    HAS_AUDIO = False
    # HAS_AUDIO = True

    core_count = runConfig.core_count
    qemu_flags = f'''
        -enable-kvm -cpu host
        -bios {OVMF_FD}
        -machine pc                # PS/2 keyboard input
        -serial file:{log_file}
        {"-s -S " if gdb else ""}
        -smp {core_count}

        -device ahci,id=ahci

        # -trace "pci_cfg_write"
        # -drive  file={DISK_IMG},if=none,id=disk1,format=raw
        # -device ide-hd,drive=disk1,bus=ahci.1
        # -nographic
    '''

    if not runConfig.auto_reboot:
        qemu_flags += " -no-reboot "

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

    # @TODO How do we timeout the test?

    cmd(f"qemu-system-x86_64 {qemu_flags}")


def make_gpt(out_path: str, deps_spec: list[tuple[str,str]]):

    fat_path = f"int/{os.path.splitext(os.path.basename(out_path))[0]}.fat"

    os.makedirs(os.path.dirname(fat_path), exist_ok=True)
    
    fat_size, _ = make_fat(fat_path, deps_spec)

    gpt_size_estimation = 2 * (2*512 + 128*128) + fat_size + (40 + 400) * 512
    cmd(f"{MKGPT} -o {out_path} --image-size {gpt_size_estimation/512} --part {fat_path} --type system")


# Returns FAT size
def make_fat(out_path: str, deps_spec: list[tuple[str,str]]):

    INT_DIR = f"int/tmp_{os.path.splitext(os.path.basename(out_path))[0]}"
    os.makedirs(INT_DIR, exist_ok=True)
    
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
                # print(f[len(d[0])-1:])
                outf = os.path.join(d[1], f[len(d[0])-1:])
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

    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    os.makedirs(INT_DIR, exist_ok=True)

    # cmd(f"dd if=/dev/zero of={out_path} bs=1k count={math.ceil(fatSize/1024)}")
    cmd(f"truncate {out_path} -s {1024*math.ceil(fatSize/1024)}")
    cmd(f"mformat -i {out_path} ::")

    # Copy files
    for src, dst in DEPS:
        assert len(dst) == 0 or dst[0] != '/', f"{src} -> {dst}"
        
        split = os.path.dirname(dst).split("/")
        acc = ""
        if len(split) > 1 or len(split[0]) > 0:
            for s in split:
                acc = os.path.join(acc, s)
                # @TODO Check if already exists. mmd fail if dir already exists.
                #    We use || true to silence error for the time being.
                cmd(f"mmd -D o -i {out_path} ::/{acc} || true")
        cmd(f"mcopy -D o -i {out_path} {src} ::/{dst}")

        int_dst = os.path.join(INT_DIR, dst)
        os.makedirs(os.path.dirname(int_dst), exist_ok=True)
        shutil.copy(src, int_dst)

    return fatSize, INT_DIR


def cmd_async(func, *args):
    thr = threading.Thread(target = func, args=args)
    thr.start()
    return thr

def wait_pool(threads):
    for t in threads:
        t.join()
    threads.clear()

def cmd(c):
    if platform.system() == "Windows":
        c = c.replace('/', "\\")
        sp = c.split(" ")
        if sp[0] == "make":
            sp[0] = "mingw32-make"
        elif not sp[0].endswith(".exe"):
            sp[0] += ".exe"
        c = " ".join(sp)
    
    if VERBOSE:
        print(c, file=sys.stderr)
    err = os.system(c)
    if err:
        print("ERR",c)
        os._exit(1)

    return 0

def cmd_back(c):
    def ext():
        cmd(c)
    cmd_async(ext)
