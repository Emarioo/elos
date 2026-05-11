#pragma once

#include <stdint.h>

typedef struct {
    uint64_t physical_address;
    uint64_t page_count;
} MemoryRegion;

typedef struct {

    //
    // Other stuff
    //
    void* rsdt; // or xsdt

    //
    //  Accessible Memory Regions
    // 
    MemoryRegion* regions; // Virtually mapped, needs to be remapped
    int           regions_len;

    // 
    //  Frame buffer we can draw to
    //
    uint8_t*  frame_buffer_base; // Virtually mapped, needs to be remapped
    uint32_t  frame_buffer_size;      // in bytes
    uint32_t  frame_buffer_width;     // in pixels
    uint32_t  frame_buffer_height;    // in pixels
    uint32_t  frame_buffer_pixels_per_scan_line;
    // Format is | blue | green | red | reserved byte |

} BootAPI;

typedef void(*FN_BootAPI)(BootAPI);
