
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

ELOS_FrameBuffer g_frame_buffer;


void draw_rect(int x, int y, int w, int h, u32 rgba) {
    if (x < 0) {
        w += x;
        x = 0;
    }
    if (y < 0) {
        h += y;
        y = 0;
    }
    if (x + w > g_frame_buffer.width)
        w = g_frame_buffer.width - x;
    if (y + h > g_frame_buffer.height)
        h = g_frame_buffer.height - y;

    u32 color = rgba;
    // TODO: SIMD
    u32* const pixels           = (u32*)g_frame_buffer.pixels;
    u32  const pixels_per_line  = g_frame_buffer.pixels_per_scan_line;
    for (int iy = y; iy < y + h; iy++) {
        for (int ix = x; ix < x + w; ix++) {
            pixels[ix + iy * pixels_per_line] = color;
        }
    }
}

#define COLOR 0xFF861394

void _start() {

    ELOS_Error error = SYS_default_monitor(&g_frame_buffer);
    if (error != ELOS_OK) {
        printf("Failed accessing monitor, %d\n", error);
    }
        #define BLOCK_SIZE 5

    draw_rect(400, 400, 80, 80, COLOR);
    while (1) {

        // Test that we can't access IO ports.
        #define STRIDE 1000000
        
        counter++;

        if (counter % STRIDE == 0) {
            // printf("Counter %d\n", counter / STRIDE);
            draw_rect(50 + BLOCK_SIZE * ((counter/STRIDE) % (g_frame_buffer.width/BLOCK_SIZE)), 50 + BLOCK_SIZE * ((counter/STRIDE) / (g_frame_buffer.width/BLOCK_SIZE)), BLOCK_SIZE, BLOCK_SIZE, COLOR);
        }
    }

    exit(0);
}


void exit(int code) {
    // @TODO Syscall to exit
    while (1) pause();
}
