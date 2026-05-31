
#include "compositor.h"

#define ELOS_SYSCALL_IMPL
#include "elos/syscalls.h"
#include "elos/common/intrinsics.h"

#include <stdarg.h>

void printf(const char* fmt, ...);
void test_messaging();

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

    test_messaging();

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


void test_messaging() {

    ELOS_Error error;
    ELOS_ServiceEndpoint endpoint;
    
    error = SYS_service_create("compositor", &endpoint, 0x1000);
    if (error != ELOS_OK) {
        printf("compositor: Could not create service.\n");

        while (1) pause();
    }
    
    printf("compositor: Created service.\n");

    while (1) {
        ELOS_ServiceEndpoint senderEndpoint;
        const u8* data;
        u64 size;
        error = SYS_service_recv(endpoint, &senderEndpoint, &data, &size, 0);
        if (!data) {
            sleep(10*1000000);
            continue;
        }

        printf("compositor: Received %d bytes, '%c%c%c%c'\n", size, data[0], data[1], data[2], data[3]);

        sleep(700*1000000);
    }
}

