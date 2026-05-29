
// gcc -o scripts/disk_fs/user_test.elf kernel/src/user/user_test.c -pie -fpic -nostdlib -nostartfiles -I kernel/src

#include "elos/common/intrinsics.h"

void exit();
void syscall_test(int value);


u64 counter;

void _start() {

    while (1) {

        // Test that we can't access IO ports.
        #define STRIDE 10000
        
        counter++;

        if (counter % STRIDE == 0) {
            syscall_test(counter/STRIDE);
        }
    }

    exit();
}


void exit() {
    // @TODO Syscall to exit
    while (1) pause();
}

void syscall_test(int value) {
    asm (
        "mov %0, %%edi\n"
        "mov $3, %%eax\n"
        "syscall\n"
        :
        : "edi" (value)
        : "rax"
    );
}
