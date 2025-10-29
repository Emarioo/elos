#!/usr/bin/env python3

import os, sys, platform, shutil, shlex

VERBOSE = False

TEST_INT = "bin/tests"

os.makedirs(TEST_INT, exist_ok=True)

def test_font_reader():
    EXE = TEST_INT + "/font_reader.exe"
    SRC = " ".join([
        "tests/font_reader.c",
        "src/elos/kernel/frame/font.c"
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