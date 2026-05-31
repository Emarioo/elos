
#pragma once

#include "elos/common/types.h"

typedef struct {
    u32  id;
    u32  width;
    u32  height;
    u32  pitch;
    u32* pixels;
} Surface;


Surface* create_surface(u32 width, u32 height);




void present_surface(Surface* surface);

