#pragma once

#include "elos/common/types.h"

typedef struct ElfObject ElfObject;

struct ElfObject {
    void* virt_image_base;
    void* phys_image_base;
    u64   image_size;
    void* entry_point;

    // void* text;
    // u64   text_size;
    // void* rodata;
    // u64   rodata_size;
    // void* data;
    // u64   data_size;
    // u64   bss_size;
};



bool read_elf(const char* path, ElfObject* object);
