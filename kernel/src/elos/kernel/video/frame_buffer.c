
#include "elos/frame_buffer.h"


typedef struct {
    uint8_t*  base;
    uint32_t  size;      // in bytes
    uint32_t  width;     // in pixels
    uint32_t  height;    // in pixels
    uint32_t  pixels_per_scan_line;
} FrameBuffer;


static FrameBuffer g_frame_buffer;

void FB_init_frame_buffer(BootAPI* boot_api) {
    g_frame_buffer.base = boot_api->frame_buffer_base;
    g_frame_buffer.size = boot_api->frame_buffer_size;
    g_frame_buffer.width = boot_api->frame_buffer_width;
    g_frame_buffer.height = boot_api->frame_buffer_height;
    g_frame_buffer.pixels_per_scan_line = boot_api->frame_buffer_pixels_per_scan_line;

    // Init font stuff?

}


