
#include "compositor.h"

#define ELOS_SYSCALL_IMPL
#include "elos/syscalls.h"
#include "elos/common/intrinsics.h"

#include <stdarg.h>

void printf(const char* fmt, ...);


void _start() {
    
    printf("Start compositor\n");

    work();

    while (1) pause();
}

void work() {

    // Loop and perform compositing work.
    // listen to move,resize events.

}

