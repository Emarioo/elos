#pragma once

#include "elos/boot_api.h"

void FB_init(BootAPI* boot_api);

void FB_printf(const char* format, ...);

void FB_write(const char* buffer, int size);

// @TODO Draw rectangles, text
