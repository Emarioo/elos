#!/usr/bin/env python3

'''

We have a collection of tests.
We want to run all, a selection or a single test.

We compile the kernel/OS once and use it for all of them.

We compile the individual tests and bake them into the kernel/OS.

We could produce a directory/package with kernel and test. In the end
we have tons of directories with duplicate kernel/OS binaries/assets and the unique tests.

Or we produce a single kernel/OS package then produce a disk_img per test?

Perhaps it depends on the type of test. Some tests, are small and can reuse same package with slight test code change.
Others require specific QEMU device flags. Some may have tons of disk images. Some none. Some multiple NICs.

We can run it on different machines, QEMU or Hardware.
For QEMU serial output works great.
For Hardware we need to communicate results with the computer.
Networking is probably the most versatile thing we can use.

We need a programatic way to reboot the test computer.
We could send a network message telling it to reboot.
Then in EFI application we ask server for image and test files.
Thinking ahead we have multiple test computers testing different things.
Test server sends AOL (awake on lan) to test machines. They then
boot up and ask server for what to load or test. This test configuration must be enabled in
the config file.

Most tests will be a C file that is compiled and linked with final kernel/OS.
This is needed so the C file can access the functions in the kernel.
Otherwise we need a runtime loader and syscalls which we don't have yet.

_start may call kernel_init and then kernel_entry. Kernel_entry is either
test function or default function that enables scheduling and default operation of the
Kernel. In some tests we don't want to enable certain things like scheduling (interferes with tests)

'''


import os, sys, platform, shutil, shlex, glob

ROOT = os.path.abspath(os.path.dirname(os.path.dirname(__file__)))

sys.path.append(ROOT)

from scripts import tools
from scripts.tools import cmd_async, wait_pool, cmd, cmd_back


VERBOSE = False

TEST_INT = "bin/tests"

os.makedirs(TEST_INT, exist_ok=True)

def test_font_reader():
    EXE = TEST_INT + "/font_reader.exe"
    SRC = " ".join([
        "tests/font_reader.c",
        "src/elos/kernel/video/font.c"
    ])
    FLAGS = "-Iinclude -Isrc -g"
    FLAGS += " -Werror=implicit-function-declaration"
    cmd(f"gcc -o {EXE} {SRC} {FLAGS}")

    cmd(f"{EXE}")



def main(args):
    global VERBOSE
    
    filters = []

    argi = 1
    while argi < len(args):
        arg = args[argi]
        argi += 1

        if arg == "-v" or arg == "--verbose":
            VERBOSE = True
        elif not arg.startswith("-"):
            filters.append(arg)
        else:
            print(f"Unknown argument '{arg}'")

    tests = []

    for filepath in glob.glob(f"{ROOT}/tests/**", recursive=True):
        basename = os.path.basename(filepath)
        if basename.startswith("test_"):
            # print("Test", filepath)
            tests.append(filepath)

    if filters:
        tests = [
            filepath
            for filepath in tests
            if any(
                test_filter in os.path.basename(filepath)
                for test_filter in filters
            )
        ]

    # @TODO Option to only re-run failed tests
        
    threads = []
    
    # cmd(f"make -f apps/libm/Makefile")
    
    INT_DIR = f"{ROOT}/int/kernel_test"

    def sync0():
        cmd(f"make -f {ROOT}/boot/Makefile INT_DIR={INT_DIR}/boot")
    
    def sync1():
        cmd(f"make -f {ROOT}/kernel/Makefile INT_DIR={INT_DIR}/kernel")
    
    threads.append(cmd_async(sync1))
    wait_pool(threads)

    threads.append(cmd_async(sync0))
    wait_pool(threads)

    total_tests = len(tests)
    passed_tests = 0

    for test_path in tests:
        print("Running", filepath)

        test_name, ext = os.path.splitext(os.path.basename(test_path))
        test_elf = f"{TEST_INT}/{test_name}.elf"

        CFLAGS = ' '.join([s.strip() for s in f'''
            -ggdb 
            -O0 -fPIC -pie
            -fno-stack-protector -fno-plt   -nostdlib -nostartfiles -nodefaultlibs
            -mno-red-zone
            -Wall -Werror -fshort-wchar -Werror=implicit-function-declaration
            -Wno-multichar
            -Wno-unused-variable -Wno-unused-function -Wno-unused-but-set-variable
            -Wl,--no-warn-execstack {ROOT}/extern/musl/libm.a
            -T{ROOT}/tests/trial_sections.ld
            -I{ROOT}/tests
            -I{ROOT}/include
            -I{ROOT}/kernel/src
            -I{ROOT}/kernel/include
            -I{ROOT}/apps/std
            -I{ROOT}/apps/prism/include
            
        '''.split("\n") if len(s) > 0])

        for filepath in glob.glob(f"{ROOT}/kernel/src/elos/common/**", recursive=True):
            # @TODO What about assembly files?
            if filepath.endswith(".c"):
                CFLAGS += f" {filepath} "

        for filepath in glob.glob(f"{ROOT}/apps/std/**", recursive=True):
            # @TODO What about assembly files?
            if filepath.endswith(".c"):
                CFLAGS += f" {filepath} "

        os.makedirs(os.path.dirname(test_elf), exist_ok=True)

        cmd(f"gcc -o {test_elf} {ROOT}/tests/trial.c {CFLAGS} {test_path}")

        package = tools.default_package(f"{TEST_INT}/releases")
        
        package.items.append(tools.PackageItem(test_elf, "PKG/TEST/TEST.ELF"))
        package.auto_run_paths = [
            "/PKG/TEST/TEST.ELF"
        ]

        tools.package_elos(package)

        # @TODO Headless flag?
        runConfig = tools.RunConfig(
            img_path = f"{package.temp_folder_path}/elos.img",
            log_path = f"{TEST_INT}/{test_name}/kernel.log",
            timeout_sec = 5,
            auto_reboot=False,
        )
        print(runConfig.log_path, runConfig.img_path)
        
        cmd(f"objdump -d {test_elf} > test.dis")

        tools.run_package(runConfig)

        with open(runConfig.log_path) as f:
            log_text = f.read()
        
        if VERBOSE:
            print(f"########  {test_name}  ##########")
            print(log_text, end="")
            if len(log_text) == 0 or log_text[-1] != '\n':
                print()

        at = log_text.find("SUCCESS 100%")
        if (at != -1):
            passed_tests += 1

    if VERBOSE:
        print(f"#########################")

    if passed_tests == total_tests:
        print(f"\033[32mTotal SUCCESS {100*passed_tests/total_tests:0.2f}% ({passed_tests}/{total_tests})\033[0m")
    else:
        print(f"\033[31mTotal FAILED {100*passed_tests/total_tests:0.2f}% ({passed_tests}/{total_tests})\033[0m")

def cmd(c):
    if platform.system() == "Windows":
        strs = shlex.split(c)
        if strs[0].startswith("./"):
            strs[0] = strs[2:]
        c = c.replace('/', "\\")
        # print(strs)
        # c = shlex.join(strs)

    if VERBOSE:
        print(c, file=sys.stderr)
    if platform.system() == "Linux":
        err = os.system(c) >> 8
    else:
        err = os.system(c)
    if err:
        if not VERBOSE:
            print("ERR",c)
        exit(1)

if __name__ == "__main__":
    main(sys.argv)