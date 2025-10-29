#pragma once 

#include "elos/kernel/frame/font/font.h"


bool font_psf__load_from_bytes(const u8* data, u32 size, Font** out_font);
