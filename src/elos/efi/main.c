
#include <efi.h>
#include <efilib.h>

#include "elos/kernel/common/core_data.h"

extern void kernel_entry();

EFI_STATUS init_protocols();

EFI_STATUS EFIAPI
efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE * SystemTable) {
    // InitializeLib(imageHandle, systemTable);
    // Print(L"Hello world!\n");

    EFI_STATUS Status;
    EFI_INPUT_KEY Key;

    /* Store the system table for future use in other functions */
    ST = SystemTable;

    BS = ST->BootServices;

    EFI_LOADED_IMAGE_PROTOCOL *loaded_image;
    // Define the GUID variable (cannot pass macro directly)
    EFI_GUID LoadedImageProtocolGUID = EFI_LOADED_IMAGE_PROTOCOL_GUID;

    // Retrieve the Loaded Image Protocol using HandleProtocol
    Status = BS->HandleProtocol(ImageHandle, &LoadedImageProtocolGUID, (void **)&loaded_image);
    if (EFI_ERROR(Status)) {
        // printf("HandleProtocol failed: 0x%lx\n", Status);
        return Status;
    }

    // Print the actual base address of the loaded image
    // printf("Image loaded at: 0x%lx\n", (uint64_t)loaded_image->ImageBase);

    // Write image base and marker for GDB
    volatile uint64_t *marker_ptr = (uint64_t *)0x10000;
    volatile uint64_t *image_base_ptr = (uint64_t *)0x10008;
    *image_base_ptr = (uint64_t)loaded_image->ImageBase;  // Store ImageBase
    *marker_ptr = 0xDEADCE11;   // Set marker

    /* Say hi */
    Status = ST->ConOut->OutputString(ST->ConOut, L"Hello World\r\n"); // EFI Applications use Unicode and CRLF, a la Windows
    if (EFI_ERROR(Status))
        return Status;

    /* Now wait for a keystroke before continuing, otherwise your
       message will flash off the screen before you see it.

       First, we need to empty the console input buffer to flush
       out any keystrokes entered before this point */
    Status = ST->ConIn->Reset(ST->ConIn, FALSE);
    if (EFI_ERROR(Status))
        return Status;

    /* Now wait until a key becomes available.  This is a simple
       polling implementation.  You could try and use the WaitForKey
       event instead if you like */
    // while ((Status = ST->ConIn->ReadKeyStroke(ST->ConIn, &Key)) == EFI_NOT_READY) ;
    (void)Key;
    
    UINTN map_size = 0;
    EFI_MEMORY_DESCRIPTOR* mem_map;
    UINTN map_key;
    UINTN desc_size;
    UINT32 desc_version;

    Status = init_protocols();
    if (EFI_ERROR(Status))
        return Status;

    // 1. First call to get required size
    Status = ST->BootServices->GetMemoryMap(&map_size, mem_map, &map_key, &desc_size, &desc_version);
    if (Status == EFI_BUFFER_TOO_SMALL) {
        // Add a little extra space (spec recommends one descriptor’s worth)
        map_size += desc_size * 2;
        
        Status = ST->BootServices->AllocatePool(EfiLoaderData, map_size, (void**)&mem_map);
        if (EFI_ERROR(Status))
            return Status;
    } else if(EFI_ERROR(Status)) {
        return Status;
    }

    // 2. Second call to actually get the map
    Status = ST->BootServices->GetMemoryMap(&map_size, mem_map, &map_key, &desc_size, &desc_version);
    if (EFI_ERROR(Status))
        return Status;

    Status = ST->BootServices->ExitBootServices(ImageHandle, map_key);
    if (EFI_ERROR(Status))
        return Status;

    // TODO: Load kernel image into memory from FAT file system.
    kernel_entry();

    // Shouldn't return. We're in the hands of the OS now
    return Status;
}

EFI_STATUS init_protocols() {
    EFI_STATUS status;
    status = ST->BootServices->LocateProtocol(&gEfiGraphicsOutputProtocolGuid, NULL, (void**)&kernel__core_data->graphics_output);
    if (EFI_ERROR(status)) return status;

    return status;
}
