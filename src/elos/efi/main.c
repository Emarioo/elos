
#include <efi.h>
#include <efilib.h>

#include "elos/kernel/common/core_data.h"
#include "elos/kernel/common/string.h"
#include "elos/kernel/memory/phys_allocator.h"
#include "elos/kernel/frame/font/font.h"
#include "elos/kernel/log/print.h"
#include "elos/kernel/debug/debug.h"

extern void kernel_entry();

EFI_STATUS init_protocols();

// Put breakpoint here
void catch_bad_status() {  }


const char* efi_memory_type_names[EfiMaxMemoryType] = {
    "EfiReservedMemoryType",
    "EfiLoaderCode",
    "EfiLoaderData",
    "EfiBootServicesCode",
    "EfiBootServicesData",
    "EfiRuntimeServicesCode",
    "EfiRuntimeServicesData",
    "EfiConventionalMemory",
    "EfiUnusableMemory",
    "EfiACPIReclaimMemory",
    "EfiACPIMemoryNVS",
    "EfiMemoryMappedIO",
    "EfiMemoryMappedIOPortSpace",
    "EfiPalCode",
    "EfiPersistentMemory",
    "EfiUnacceptedMemoryType",
};


EFI_STATUS fetch_memory_map() {
    EFI_STATUS Status;
    if(g_memory_mapper.memory_descriptors) {
        Status = ST->BootServices->FreePool(g_memory_mapper.memory_descriptors);
        if(EFI_ERROR(Status)) {
            catch_bad_status();
            return Status;
        }
        g_memory_mapper.memory_descriptors = NULL;
    }

    // 1. First call to get required size
    Status = ST->BootServices->GetMemoryMap(&g_memory_mapper.total_size_of_descriptors, g_memory_mapper.memory_descriptors, &g_memory_mapper.map_key, &g_memory_mapper.descriptor_size, &g_memory_mapper.desc_version);
    if (Status == EFI_BUFFER_TOO_SMALL) {
        // Add a little extra space (spec recommends one descriptor’s worth)
        g_memory_mapper.total_size_of_descriptors += g_memory_mapper.descriptor_size * 2;
        
        Status = ST->BootServices->AllocatePool(EfiLoaderData, g_memory_mapper.total_size_of_descriptors, (void**)&g_memory_mapper.memory_descriptors);
        if (EFI_ERROR(Status)) {
            catch_bad_status();
            return Status;
        }
    } else if(EFI_ERROR(Status)) {
        catch_bad_status();
        return Status;
    }

    // 2. Second call to actually get the map
    Status = ST->BootServices->GetMemoryMap(&g_memory_mapper.total_size_of_descriptors, g_memory_mapper.memory_descriptors, &g_memory_mapper.map_key, &g_memory_mapper.descriptor_size, &g_memory_mapper.desc_version);
    if (EFI_ERROR(Status)) {
        catch_bad_status();
        return Status;
    }
    return Status;
}

EFI_STATUS load_font();

EFI_STATUS print_memory_map() {
    #define IS_FREE_PAGE(N) ((N == EfiConventionalMemory) || (N == EfiPersistentMemory))
    // We want to keep these (N >= EfiLoaderCode && N <= EfiBootServicesData)

    int reserved_pages;
    int free_pages;
    for (int i = 0; i < g_memory_mapper.total_size_of_descriptors/g_memory_mapper.descriptor_size; i++) {
        EFI_MEMORY_DESCRIPTOR* region = (EFI_MEMORY_DESCRIPTOR*)((char*)g_memory_mapper.memory_descriptors + i*g_memory_mapper.descriptor_size);
        
        int free = IS_FREE_PAGE(region->Type);
        if (free) {
            free_pages += region->NumberOfPages;
        } else {
            reserved_pages += region->NumberOfPages;
        }

        printf("Region %d, phys: %d, pages: %d, virt: %d\r\n", i, (int)region->PhysicalStart, (int)region->NumberOfPages, (int)region->VirtualStart);
        printf("  type %s, attr: %d\r\n", efi_memory_type_names[region->Type], (int)region->Attribute);
    }
    
    printf("Reserved pages: %d\r\n", reserved_pages);
    printf("Free pages: %d\r\n", free_pages);
    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI
efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE * SystemTable) {
    // InitializeLib(imageHandle, systemTable);
    // Print(L"Hello world!\n");


    EFI_STATUS Status;
    EFI_INPUT_KEY Key;

    /* Store the system table for future use in other functions */
    ST = SystemTable;

    BS = ST->BootServices;
    kernel__core_data->inside_uefi = true;

    EFI_LOADED_IMAGE_PROTOCOL *loaded_image;
    // Define the GUID variable (cannot pass macro directly)
    EFI_GUID LoadedImageProtocolGUID = EFI_LOADED_IMAGE_PROTOCOL_GUID;

    // Retrieve the Loaded Image Protocol using HandleProtocol
    Status = BS->HandleProtocol(ImageHandle, &LoadedImageProtocolGUID, (void **)&loaded_image);
    if (EFI_ERROR(Status)) {
        catch_bad_status();
        // printf("HandleProtocol failed: 0x%lx\n", Status);
        return Status;
    }

    Status = BS->HandleProtocol(loaded_image->DeviceHandle, &gEfiSimpleFileSystemProtocolGuid, (void**)&kernel__core_data->simple_file_system);
    if (EFI_ERROR(Status)) {
        catch_bad_status();
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
    printf("Hello World\r\n"); // EFI Applications use Unicode and CRLF, a la Windows

    /* Now wait for a keystroke before continuing, otherwise your
       message will flash off the screen before you see it.

       First, we need to empty the console input buffer to flush
       out any keystrokes entered before this point */
    Status = ST->ConIn->Reset(ST->ConIn, FALSE);
    if (EFI_ERROR(Status)) {
        catch_bad_status();
        return Status;
    }

    /* Now wait until a key becomes available.  This is a simple
       polling implementation.  You could try and use the WaitForKey
       event instead if you like */
    // while ((Status = ST->ConIn->ReadKeyStroke(ST->ConIn, &Key)) == EFI_NOT_READY) ;
    (void)Key;
    


    Status = init_protocols();
    if (EFI_ERROR(Status)) {
        catch_bad_status();
        return Status;
    }

    Status = fetch_memory_map();
    if (EFI_ERROR(Status)) {
        return Status;
    }

    print_memory_map();

    
    Status = load_font();
    if (EFI_ERROR(Status)) {
        return Status;
    }

    // Need to get new memory map

    Status = fetch_memory_map();
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Status = ST->BootServices->ExitBootServices(ImageHandle, g_memory_mapper.map_key);
    if (EFI_ERROR(Status)) {
        catch_bad_status();
        printf("ExitBootServices failed\r\n");
        return Status;
    }

    serial_printf("UEFI - Exit boot services\n");

    kernel__core_data->inside_uefi = false;

    bool yes = kernel_init_memory_mapper();
    if (!yes)
        printf("bad\r\n");

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

EFI_STATUS list_content(EFI_FILE_PROTOCOL* dir, int depth, EFI_FILE_INFO* info);


EFI_STATUS load_font() {
    EFI_STATUS Status;
    EFI_FILE_PROTOCOL* volume;
    Status = kernel__core_data->simple_file_system->OpenVolume(kernel__core_data->simple_file_system, &volume);
    if (EFI_ERROR(Status)) {
        catch_bad_status();
        return Status;
    }
    
    
    // Iterate files/dirs in folder
    Status = list_content(volume, 0, NULL);
    if (EFI_ERROR(Status))
        return Status;

    // const char* font_path = "\\RES\\PIXELOP.TTF";
    const char* font_path = "\\RES\\STDFONT.PSF";
    u16* font_wpath = tmp_path_wstring(font_path);

    EFI_FILE_PROTOCOL* handle;
    Status = volume->Open(volume, &handle, font_wpath, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(Status)) {
        catch_bad_status();
        return Status;
    }

    char temp_buffer[sizeof(EFI_FILE_INFO) + 256];
    EFI_FILE_INFO* file_info = (EFI_FILE_INFO*)temp_buffer;
    UINTN buffer_size = sizeof(EFI_FILE_INFO) + 256;

    Status = handle->GetInfo(handle, &gEfiFileInfoGuid, &buffer_size, file_info);
    if (EFI_ERROR(Status)) {
        catch_bad_status();
        return Status;
    }
    const int PAGE_SIZE = 4096;
    int pages = (file_info->FileSize + PAGE_SIZE-1) / PAGE_SIZE;

    EFI_PHYSICAL_ADDRESS address;
    Status = ST->BootServices->AllocatePages(AllocateAnyPages, EfiLoaderData, pages, &address);
    if (EFI_ERROR(Status)) {
        catch_bad_status();
        return Status;
    }

    UINTN file_size = file_info->FileSize;
    Status = handle->Read(handle, &file_size, (void*)address);
    if (EFI_ERROR(Status)) {
        catch_bad_status();
        return Status;
    }

    bool res = font__load_from_bytes((u8*)address, file_size, &g_default_font);
    if (!res) {
        printf("Could not load font");
        return -1;
    }

    return EFI_SUCCESS;
}


EFI_STATUS list_content(EFI_FILE_PROTOCOL* dir, int depth, EFI_FILE_INFO* info) {
    EFI_STATUS Status;
    UINTN buf_size;
    if (!info) {
        buf_size  = sizeof(EFI_FILE_INFO) + 256;
        Status = ST->BootServices->AllocatePool(EfiLoaderData, buf_size, (void**)&info);
        if(EFI_ERROR(Status)) {
            catch_bad_status();
            return Status;
        }
    }

    char filename[256];

    // Iterate files/dirs in folder
    while (1) {
        buf_size = sizeof(EFI_FILE_INFO) + 256;
        Status = dir->Read(dir, &buf_size, info);
        if (EFI_ERROR(Status) || buf_size == 0)
            break;

        if (info->FileName[0] == '.')
            continue;

        int i=0;
        while(info->FileName[i]) {
            filename[i] = (char)info->FileName[i];
            i++;
        }
        filename[i] = '\0';

        
        EFI_FILE_PROTOCOL* handle;
        Status = dir->Open(dir, &handle, info->FileName, EFI_FILE_MODE_READ, 0);
        if (EFI_ERROR(Status)) {
            catch_bad_status();
            return Status;
        }
        
        for(i=0;i<depth;i++) {
            printf(" ");
        }
        if (info->Attribute & EFI_FILE_DIRECTORY) {
            printf("%s\r\n", filename);
            Status = list_content(handle, depth+1, info);
            if(EFI_ERROR(Status))
                return Status;
        } else {
            printf("%s %d\r\n", filename, info->FileSize);
        }
        handle->Close(handle);
    }
    dir->SetPosition(dir, 0);

    return EFI_SUCCESS;
}
