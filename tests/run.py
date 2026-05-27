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


import os, sys, platform, shutil, shlex

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

def main():
    test_font_reader();

if __name__ == "__main__":
    main()