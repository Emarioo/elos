
#include <efi.h>
#include <efilib.h>

typedef struct kernel__CoreData {
    EFI_GRAPHICS_OUTPUT_PROTOCOL* graphics_output;
} kernel__CoreData;

#define kernel__core_data ((kernel__CoreData*)0x88000)
