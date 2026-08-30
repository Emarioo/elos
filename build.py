#!/usr/bin/env python3

'''
This script compiles all tools/binaries by default.
For the time being we also run kernel in QEMU but we probably won't later.

The following tools/binaries exist:
- elos.img

'''

import os, sys, math, re, platform, shutil, shlex, glob, math, threading, multiprocessing, dataclasses, subprocess
from dataclasses import dataclass



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
        package_elos("releases", iso)


    if netboot:
        wait_pool([net_thread])
        cmd(f"{netboot_server_bin}")

    elif run:
        # TODO: DON'T HARDCODE PATHS
        OVMF_FD = "extern/ovmf/OVMF.fd"

        DISK_IMG = "int/disk.img"
        # # if not os.path.exists(DISK_IMG):
            
        # DEPS_SPEC: list[tuple[str,str]] = [
        #     ("scripts/disk_fs/*", ""),
        # ]
        # make_gpt(DISK_IMG, DEPS_SPEC)

            # cmd(f"qemu-img create -f raw {DISK_IMG} 64M")
            # cmd(f"gcc scripts/fwrite.c -g -o int/fwrite && int/fwrite {DISK_IMG}")
        HAS_AUDIO = True
        core_count = 1
        qemu_flags = f'''
            -enable-kvm -cpu host
            -bios {OVMF_FD}
            -machine pc                # PS/2 keyboard input
            -serial file:bin/kernel.log 
            -s 
            {"-S " if gdb else ""}
            -smp {core_count}

            -device ahci,id=ahci

            # -M q35

            # -trace "pci_cfg_write"
            # -drive  file={DISK_IMG},if=none,id=disk1,format=raw
            # -device ide-hd,drive=disk1,bus=ahci.1
            # -nographic
        '''
        if HAS_AUDIO:
            qemu_flags += f'''
            -device intel-hda #,debug=4
            -device hda-output
            # -device hda-duplex # includes microphone
            '''
        if HAS_TAP:
            qemu_flags += f'''
            -device e1000,netdev=net0  # e1000 ~= intel 8254x
            -netdev tap,id=net0,ifname=tap0,script=no,downscript=no
            #-netdev user,id=net0
            '''

        # @NOTE Not sure what these flags do but seems useful/important
        # Use -L if you want whole firmware package (seems to use secure boot with test keys)
        # f"-L /usr/share/ovmf/ "
        # f"-drive format=raw,file=bin/OVMF.fd,if=pflash "   # -pflash (but without warnings)

        if not iso:
            qemu_flags += f'''
                -drive file=bin/elos.img,if=none,id=disk0,format=raw
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


def package_elos(release_dir, build_iso = False):
    name    = "elos"
    version = "0.0.1"
    arch    = "x86_64"

    temp_folder_name = f"{name}-{version}-{arch}"
    temp_folder_path = f"{release_dir}/{temp_folder_name}"

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
    prism_path      = f"{temp_folder_path}/initrd/pkg/prism/prism.elf"
    term_path       = f"{temp_folder_path}/initrd/pkg/term/term.elf"
    slate_path      = f"{temp_folder_path}/initrd/pkg/slate/slate.elf"
    
    win32_loader    = f"{temp_folder_path}/initrd/pkg/win32_loader/win32_loader.elf"
    wintest    = f"{temp_folder_path}/initrd/pkg/win32_loader/wintest.exe"
    
    doom_path       = f"{temp_folder_path}/initrd/pkg/doom/doom.elf"
    wad_path        = f"{temp_folder_path}/initrd/pkg/doom/doom1.wad"

    DOOM_MAKEFILE   = f"{ROOT}/../doomgeneric/doomgeneric/Makefile.elos"
    DOOM_WAD        = f"{ROOT}/../iwad/doom1.wad"

    # @TODO Provide copyright-free sound for testing.
    WAV_PATH        = f"{ROOT}/dream_aria.wav"

    provide_doom = os.path.exists(DOOM_MAKEFILE) and os.path.exists(DOOM_WAD)

    if provide_doom:
        os.makedirs(os.path.dirname(wad_path), exist_ok=True)
        cmd(f"cp {DOOM_WAD} {wad_path}")

    INT_DIR         = f"{ROOT}/int"
    fat_path        = f"{INT_DIR}/fat.img"


    threads = []

    def sync0():
        cmd(f"make -f {ROOT}/boot/Makefile INT_DIR={INT_DIR}/boot BOOT_EFI={bootx64_path}")
    
    def sync1():
        cmd(f"make -f {ROOT}/kernel/Makefile INT_DIR={INT_DIR}/kernel KERNEL_IMAGE={kernel_path} KERNEL_ELF={kernel_elf_path}")
    
    def sync2():
        cmd(f"APP_OUTPUT={prism_path} make -f apps/prism/Makefile")
        cmd_back(f"objdump -S {prism_path} > prism.dis")
        
    def sync3():
        cmd(f"APP_OUTPUT={term_path} make -f apps/terminal/Makefile")
        cmd_back(f"objdump -S {term_path} > term.dis")

    def sync4():
        cmd(f"APP_OUTPUT={slate_path} make -f apps/slate/Makefile")
        cmd_back(f"objdump -S {slate_path} > slate.dis")

    def sync5():
        cmd(f"OUTPUT={doom_path} make -f {DOOM_MAKEFILE}")
        cmd_back(f"objdump -S {doom_path} > doom.dis")

    def sync6():
        cmd(f"APP_OUTPUT={win32_loader} make -f apps/win32_loader/Makefile")
        cmd_back(f"objdump -S {win32_loader} > win32.dis")
        
    def sync7():
        cmd(f"APP_OUTPUT2={wintest} make -f apps/win32_loader/Makefile wintest")
        cmd_back(f"objdump -S {wintest} > wintest.dis")
    
    threads.append(cmd_async(sync0))
    threads.append(cmd_async(sync1))

    wait_pool(threads)
    
    threads = []
    threads.append(cmd_async(sync2))
    threads.append(cmd_async(sync3))
    threads.append(cmd_async(sync4))
    threads.append(cmd_async(sync6))
    threads.append(cmd_async(sync7))
    if provide_doom:
        threads.append(cmd_async(sync5))

    wait_pool(threads)


    DEPS_SPEC: list[tuple[str,str]] = [
        (prism_path, "PKG/PRISM/PRISM.ELF"),
        (term_path,  "PKG/TERM/TERM.ELF"),
        (slate_path, "PKG/SLATE/SLATE.ELF"),
        (win32_loader, "PKG/win32_loader/win32_loader.ELF"),
        (wintest, "PKG/win32_loader/wintest.exe"),
    ]
    if provide_doom:
        DEPS_SPEC.append((doom_path, "PKG/DOOM/DOOM.ELF"))
        DEPS_SPEC.append((wad_path, "PKG/DOOM/DOOM1.WAD"))

    if os.path.exists(WAV_PATH):
        DEPS_SPEC.append((WAV_PATH, "PKG/WAV/DREAM.WAV"))
    else:
        print(f"\033[33mWARNING:\033[0m Could not find {WAV_PATH} (it won't be included in PKG)")
        
    make_gpt(initrd_path, DEPS_SPEC)
    
    DEPS_SPEC: list[tuple[str,str]] = [
        (bootx64_path, "EFI/BOOT/BOOTX64.EFI"),
        (kernel_path, "KERNEL.IMG"),
        (initrd_path, "INITRD.IMG"),
        ("boot/template.cfg", "TEMPLATE.CFG"),
        ("res/Lat2-Terminus16.psf", "STDFONT.PSF"),  # baked into kernel image and not needed.
                                                     # We currently don't package files with user apps
                                                     # so we provide this default font in a file.
    ]

    fat_size, ISO_DIR = make_fat(fat_path, DEPS_SPEC)

    
        
    if build_iso:
        def sync1():
            cmd(f"cp {fat_path} {ISO_DIR}/fat.img")
            cmd(f"xorriso -as mkisofs -R -f -e fat.img -no-emul-boot -o {iso_path} {ISO_DIR}")
            if os.path.exists(iso_path):
                cmd(f"cp {iso_path} bin/elos.iso")
        threads.append(cmd_async(sync1))

    #                       GPT header info      fat    some extra rom
    gpt_size_estimation = 2 * (2*512 + 128*128) + fat_size + (40 + 400) * 512
    cmd(f"{MKGPT} -o {img_path} --image-size {gpt_size_estimation/512} --part {fat_path} --type system")

    # cmd(f"xorriso -as mkisofs -R -f -no-emul-boot -o {iso_path} {ISO_DIR}")

    # Copy ISO into folder

    # Copy raw image into folder

    # Zip folder

    def sync3():
        cmd(f"cd {os.path.dirname(temp_folder_path)} && tar -czf {temp_folder_name}.tar.gz {temp_folder_name}")

    cmd_async(sync3)

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

    cmd(f"dd if=/dev/zero of={out_path} bs=1k count={math.ceil(fatSize/1024)}")
    # cmd(f"truncate {out_path} -s {1024*math.ceil(fatSize/1024)}")
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

def cmd_async(func):
    thr = threading.Thread(target = func)
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

if __name__ == "__main__":
    main()