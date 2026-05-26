
#include "elos/common/types.h"
#include "elos/common/intrinsics.h"
#include "elos/physical_memory.h"

#include "elos/kernel_console.h"


#define printf(...) KCON_printf(__VA_ARGS__)

#define STBI_MALLOC  wrap_stbi_malloc
#define STBI_REALLOC wrap_stbi_realloc
#define STBI_FREE    wrap_stbi_free

// We only want PNG and BMP at the moment
#define STBI_NO_LINEAR
#define STBI_NO_GIF
// #define STBI_NO_JPEG
#define STBI_NO_HDR
#define STBI_NO_TGA
#define STBI_NO_PSD
#define STBI_NO_PIC
#define STBI_NO_PNM
#define STBI_NO_THREAD_LOCALS

#define STBI_ASSERT(x) wrap_stbi_assert(x)


void wrap_stbi_assert(int x) {
    if (!x) {
        printf("STBI asserted\n");
        while (1) pause();
    }
}

void* wrap_stbi_malloc(u64 size) {
    return PMEM_alloc(size);
}

void* wrap_stbi_realloc(void* ptr, u64 size) {
    return PMEM_realloc(size, ptr);
}

void wrap_stbi_free(void* ptr) {
    PMEM_free(ptr);
}

#define STBI_NO_STDIO
#define STB_IMAGE_IMPLEMENTATION
#include "vendor/stb_image.h"

int abs(int x) {
    return x < 0 ? -x : x;
}
