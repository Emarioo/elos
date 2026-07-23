#pragma once 

#include "slate/font/font.h"

#include "elos/common/allocator.h"


bool font_psf__load_from_bytes(const u8* data, u32 size, Font** out_font, Allocator* allocator);
