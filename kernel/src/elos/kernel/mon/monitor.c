
#include "elos/monitor.h"

#include "elos/physical_memory.h"

MON_FrameBuffer g_defaultFrameBuffer;

void MON_init(BootAPI* boot_api) {
    if (boot_api->frame_buffer_base) {
        g_defaultFrameBuffer.phys_address = (void*)boot_api->frame_buffer_base;
        g_defaultFrameBuffer.size = boot_api->frame_buffer_size;
        g_defaultFrameBuffer.width = boot_api->frame_buffer_width;
        g_defaultFrameBuffer.height = boot_api->frame_buffer_height;
        g_defaultFrameBuffer.pixels_per_scan_line = boot_api->frame_buffer_pixels_per_scan_line;
    }
}


/*
    Scans the computer for monitors.
*/
void MON_scan_devices(MonitorDevice devices[], int* count) {
    if (!count)
        return;
    if (!g_defaultFrameBuffer.phys_address) {
        *count = 0;
        return;
    }
    if (!devices || *count <= 0) {
        *count = 1;
        return;
    }
    devices[0] = &g_defaultFrameBuffer;
    *count = 1;
}

bool MON_get_frame_buffer(MonitorDevice device, MON_FrameBuffer* frameBuffer) {
    if (device == &g_defaultFrameBuffer) {
        *frameBuffer = g_defaultFrameBuffer;
        return true;
    }
    return false;
}
