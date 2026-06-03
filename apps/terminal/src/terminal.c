/*
    Terminal implementation:

        1. Init compositor connection.

        2. Request a surface/window.

        3. Read keyboard input and perform action (write text and execute command)

        4. Draw text on the surface

        5. Present surface to compositor

        6. Repeat 3
            
*/

#include "prism/prism.h"

#include <stdint.h>

#define ELOS_SYSCALL_IMPL
#include "elos/syscalls.h"

#include "elos/common/intrinsics.h"


// From apps/std/stdio.c (uses SYS_debug_log)
void printf(const char* format, ...);

void exit(int exitCode);

void sleep(u64 ns);

void terminal_loop();

void draw_rect(int x, int y, int w, int h, uint32_t rgba);

#define BLACK 0xFF000000
#define RED 0xFFD91938

PrismInstance* g_instance;
PrismSurface* g_surface;

PrismSurfaceInfo g_surfaceInfo;

u64 ticks_per_second;

ELOS_UserEventBuffer* userEvents;

bool get_event(ELOS_UserEvent* event) {
    // @TODO Not thread or context switch safe.
    u64 tail = userEvents->tail % userEvents->maxEvents;
    u64 head = userEvents->head % userEvents->maxEvents;
    if (tail == head) {
        return false;
    }
    *event = userEvents->events[tail];
    userEvents->tail++;
    return true;
}

void _start() {

    SYS_ticks_per_second(&ticks_per_second);

    g_instance = prism_init();
    if (!g_instance) {
        printf("terminal: Could not init PRISM client\n");
        exit(1);
    }

    g_surface = prism_createSurface(g_instance, 800, 600);
    if (!g_surface) {
        printf("terminal: Could not create surface\n");
        exit(1);
    }

    prism_surfaceInfo(g_surface, &g_surfaceInfo);

    prism_moveSurface(g_surface, 200, 200);

    ELOS_Error error;
    error = SYS_request_user_event_buffer(10000, &userEvents);
    if (error != ELOS_OK) {
        printf("terminal: Could not create user event buffer\n");
        exit(1);
    }


    terminal_loop();
}



void terminal_loop() {


    int x = 10;
    int y = 10;
    int size = 20;
    int padding = 2;
    int velx = 1;
    int vely = 1;

    while (1) {
        ELOS_UserEvent event;
        bool has = get_event(&event);
        if (has) {
            printf("type=%d id=%d chr=%c pressed=%d\n", event.type, event.id, event.key.character, event.key.value);
        }

        draw_rect(x, y, size, size, BLACK);
        draw_rect(x+padding, y+padding, size-2*padding, size-2*padding, RED);

        x += velx;
        y += vely;

        if (x+size > g_surfaceInfo.width || x < 0) {
            velx *= -1;
        }
        if (y+size > g_surfaceInfo.height || y < 0) {
            vely *= -1;
        }

        prism_presentSurface(g_surface);

        // sleep((1000/144)*1000000);
        sleep((1000/60)*1000000);
    }

}





void draw_rect(int x, int y, int w, int h, uint32_t rgba) {
    if (x < 0) {
        w += x;
        x = 0;
    }
    if (y < 0) {
        h += y;
        y = 0;
    }
    if (x + w > g_surfaceInfo.width)
        w = g_surfaceInfo.width - x;
    if (y + h > g_surfaceInfo.height)
        h = g_surfaceInfo.height - y;

    uint32_t* const pixels           = g_surfaceInfo.buffer;
    uint32_t  const pixels_per_line  = g_surfaceInfo.stride;
    for (int iy = y; iy < y + h; iy++) {
        for (int ix = x; ix < x + w; ix++) {
            pixels[ix + iy * pixels_per_line] = rgba;
        }
    }
}




void sleep(u64 ns) {
    u64 start = rdtsc();
    while (1) {
        u64 now = rdtsc();
        u64 now_ns = (1000000000 * (now - start)) / ticks_per_second;
        if (now_ns > ns) {
            return;
        }
    }
}

void exit(int exitCode) {
    // @TODO Implement syscall to exit.
    while (1) pause();
}