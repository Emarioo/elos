#pragma once

#include <efi.h>
#include <efilib.h>

typedef struct kernel__CoreData {
    int exited_bootservices;
    EFI_GRAPHICS_OUTPUT_PROTOCOL* graphics_output;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* simple_file_system;
} kernel__CoreData;

#define kernel__core_data ((kernel__CoreData*)0x88000)
