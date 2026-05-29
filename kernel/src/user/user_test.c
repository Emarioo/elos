
// gcc -o scripts/disk_fs/user_test.elf kernel/src/user/user_test.c kernel/src/elos/common/string.c -pie -fpic -nostdlib -nostartfiles -I kernel/src -I kernel/include -Wno-builtin-declaration-mismatch

#include "elos/common/intrinsics.h"

#define ELOS_SYSCALL_IMPL

#include "elos/syscalls.h"

#include "elos/common/string.h"

#include <stdarg.h>

void exit(int code);
void syscall_test(int value);


void printf(const char* format, ...) {
    char buffer[256];

    va_list va;
    va_start(va, format);
    int len = vsnprintf(buffer, sizeof(buffer), format, va);
    va_end(va);

    SYS_debug_log(buffer, len);
}



u64 counter;

void _start() {

    while (1) {

        // Test that we can't access IO ports.
        #define STRIDE 10000
        
        counter++;

        if (counter % STRIDE == 0) {
            printf("Counter %d\n", counter / STRIDE);
        }
    }

    exit(0);
}


void exit(int code) {
    // @TODO Syscall to exit
    while (1) pause();
}
