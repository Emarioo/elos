#pragma once

#include "elos/boot_api.h"

#include "elos/common/types.h"


typedef struct {
    u8*  base;
    u32  size;      // in bytes
    u32  width;     // in pixels
    u32  height;    // in pixels
    u32  pixels_per_scan_line;
} FrameBuffer;

extern FrameBuffer g_frame_buffer;

void FB_init(BootAPI* boot_api);

void FB_printf(const char* format, ...);

void FB_write(const char* text, int len);

// @TODO Draw rectangles, text
