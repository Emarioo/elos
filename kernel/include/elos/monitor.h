#pragma once

#include "elos/boot_api.h"

#include "elos/common/types.h"


typedef void* MonitorDevice;

typedef struct {
    u32  width;
    u32  height;
    u32  size;
    u32  pixels_per_scan_line;
    u32* phys_address;
} MON_FrameBuffer;

void MON_init(BootAPI* boot_api);


/*
    Scans the computer for monitors.
*/
void MON_scan_devices(MonitorDevice devices[], int* count);

bool MON_get_frame_buffer(MonitorDevice device, MON_FrameBuffer* frameBuffer);

