
#include "compositor.h"

#define ELOS_SYSCALL_IMPL
#include "elos/syscalls.h"
#include "elos/common/intrinsics.h"

#include <stdarg.h>

void printf(const char* fmt, ...);


u64 tsc_per_sec;

void sleep(u64 ns) {
    u64 start = rdtsc();
    while (1) {
        u64 now = rdtsc();
        if (now - start > (tsc_per_sec*ns)/1000000000 )
            break;
        pause();
    }
}


void _start() {
    
    printf("Start compositor\n");
    SYS_ticks_per_second(&tsc_per_sec);

    while (1) {
        printf("Running compositor\n");
        sleep(200*1000000);
    }

    work();

    while (1) pause();
}

void work() {

    // Loop and perform compositing work.
    // listen to move,resize events.

}

