
// gcc -o scripts/disk_fs/user_test.elf kernel/src/user/user_test.c kernel/src/elos/common/string.c -pie -fpic -nostdlib -nostartfiles -I kernel/src -I kernel/include -Wno-builtin-declaration-mismatch

#include "elos/common/intrinsics.h"

#define ELOS_SYSCALL_IMPL
#include "elos/syscalls.h"

#include "elos/common/string.h"

#include <stdarg.h>

void exit(int code);
void test_messaging();

void printf(const char* format, ...);



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

    SYS_ticks_per_second(&tsc_per_sec);
    printf("Starting terminal\n");

    test_messaging();

    while (1) {
        printf("Running terminal\n");
        sleep(500*1000000);
    }

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

void test_messaging() {
    ELOS_Error error;
    ELOS_ServiceEndpoint endpoint;
    while (1) {
        error = SYS_service_connect("compositor", &endpoint, 0x1000);
        if (error == ELOS_OK)
            break;
        printf("terminal: Waiting for compositor service...\n");
        sleep(500*1000000);
    }

    printf("terminal: Established compositor endpoint.\n");

    char message[256];
    int len = snprintf(message, sizeof(message), "hello");

    error = SYS_service_send(endpoint, message, len + 1);
    if (error != ELOS_OK) {
        printf("terminal: Could not send 'hello'\n");
        while (1) pause();
    }

    const u8* data;
    u64 data_size;
    while (1) {
        error = SYS_service_recv(endpoint, NULL, &data, &data_size, 0);
        if (error != ELOS_OK || !data) {
            printf("terminal: no recv, %d, %x %d\n", error, data, data_size);
            pause();
            sleep(100*1000000);
            continue;
        }
        break;
    }

    typedef struct {
        char magic[4];
        ELOS_SharedMemoryHandle handle;
    } CompositorHeader;

    CompositorHeader* header = (void*)data;
    
    void* memory;
    u64   memory_size;
    error = SYS_shared_memory_info(header->handle, &memory, &memory_size);
    if (error != ELOS_OK) {
        printf("terminal: SYS_shared_memory_info error, %d\n", error);
        while (1) pause();
    }

    printf("terminal: Received shared memory: %c%c%c%c, %x\n",
        header->magic[0], header->magic[1], header->magic[2], header->magic[3], header->handle);

    typedef struct {
        volatile u32 term_counter;
        volatile u32 comp_counter;
        volatile u32 both_counter;
    } SHM;

    SHM* shm = memory;

    while (1) {
        __atomic_fetch_add(&shm->term_counter, 1, __ATOMIC_SEQ_CST);
        __atomic_fetch_add(&shm->both_counter, 1, __ATOMIC_SEQ_CST);
        
        printf("terminal: %d %d %d\n", shm->term_counter, shm->comp_counter, shm->both_counter);

        sleep(10*1000000);
    }
}


void exit(int code) {
    // @TODO Syscall to exit
    while (1) pause();
}
