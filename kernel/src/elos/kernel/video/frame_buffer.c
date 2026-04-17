
#include "elos/frame_buffer.h"

#include "elos/common/types.h"




FrameBuffer g_frame_buffer;

void FB_init(BootAPI* boot_api) {
    g_frame_buffer.base = boot_api->frame_buffer_base;
    g_frame_buffer.size = boot_api->frame_buffer_size;
    g_frame_buffer.width = boot_api->frame_buffer_width;
    g_frame_buffer.height = boot_api->frame_buffer_height;
    g_frame_buffer.pixels_per_scan_line = boot_api->frame_buffer_pixels_per_scan_line;

    // Init font stuff?

}




void FB_printf(const char* format, ...) {
    
}

void FB_write(const char* buffer, int size) {

}
