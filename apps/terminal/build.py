
import os, sys, platform

ELOS_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(__file__)))
PROG_ROOT = os.path.dirname(__file__)
APPS_ROOT = os.path.dirname(os.path.dirname(__file__))

PRISM_ROOT = f"{APPS_ROOT}/prism"

VERBOSE = False

def main(OUTPUT = "scripts/disk_fs/term.elf"):
    SRC_FILES = [
        f"{PROG_ROOT}/src/terminal.c",
        f"{PRISM_ROOT}/src/prism_client/prism_client.c",
        f"{APPS_ROOT}/std/stdio.c",
        f"{ELOS_ROOT}/kernel/src/elos/common/string.c",
        f"{ELOS_ROOT}/kernel/src/elos/common/string_fast.s",
    ]

    SRC_FILES = " ".join(SRC_FILES)
    FLAGS  = "-g -pie -fpic -nostdlib -nostartfiles"
    FLAGS += " -Wno-builtin-declaration-mismatch"
    FLAGS += f" -I kernel/src -I kernel/include -I {PROG_ROOT} -I {PRISM_ROOT}/include "

    cmd(f"gcc -o {OUTPUT} {SRC_FILES} {FLAGS}")


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
        if not VERBOSE:
            print("ERR",c)
        exit(1)

    return 0


if __name__ == "__main__":
    main()