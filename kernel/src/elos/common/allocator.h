#pragma once

#include "elos/common/types.h"

typedef struct Allocator Allocator;

typedef void*(*FN_Allocate)(Allocator*, u64, void*);

struct Allocator {
    FN_Allocate allocate;
    void* user_data;
};
