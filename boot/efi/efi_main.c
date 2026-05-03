/*
    EFI Application to load kernel into memory.
*/


#include <efi.h>
#include <efilib.h>

#include "elos/boot_api.h"
#include "elos/common/string.h"
#include "elos/common/intrinsics.h"

#include "elos/netboot.h"

/*
    Types
*/


/*
    Constants
*/


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


/*
    Global variables
*/

EFI_GRAPHICS_OUTPUT_PROTOCOL* graphics_output;
EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* simple_file_system;
EFI_SIMPLE_NETWORK_PROTOCOL* simple_network;
EFI_LOADED_IMAGE_PROTOCOL* loaded_image;

UINTN g_last_map_key; // needed when allocating memory and exiting boot services.
// This is the only data we pass to the kernel
BootAPI g_boot_api;



/*
    Function Declarations
*/

EFI_STATUS boot_init_memory();

void boot_init_frame_buffer();

EFI_STATUS load_kernel();

// Put breakpoint here

// EFI_STATUS load_font();


/*
    Function Definitions
*/

void catch_bad_status() {  }

void printf(const char* format, ...) {
    char buffer[256];
    unsigned short w_buffer[256];

    va_list va;
    va_start(va, format);
    const int len = vsnprintf(buffer, sizeof(buffer), format, va);
    va_end(va);

    for (int i=0;i<len+1;i++) {
        w_buffer[i] = buffer[i];
    }
    EFI_STATUS status = ST->ConOut->OutputString(ST->ConOut, w_buffer);
    (void)status;
}


EFI_STATUS boot_init_memory() {
    EFI_STATUS Status;

    MemoryRegion* regions = NULL;
    int           regions_len = 0;

    EFI_MEMORY_DESCRIPTOR* memory_descriptors = NULL;
    UINTN descriptor_size = 0;
    UINTN total_size_of_descriptors = 0;
    UINTN map_key = 0;
    UINT32 desc_version = 0;
    
    // First get size we need to allocate for descriptors
    Status = ST->BootServices->GetMemoryMap(&total_size_of_descriptors, memory_descriptors, &map_key, &descriptor_size, &desc_version);
    if (Status != EFI_BUFFER_TOO_SMALL) {
        printf("GetMemoryMap error %d\r\n", Status);
        catch_bad_status();
        return Status;
    }

    // Add a little extra space (spec recommends one descriptor’s worth)
    total_size_of_descriptors += descriptor_size * 2;

    // Allocate memory for descriptors    
    Status = ST->BootServices->AllocatePool(EfiLoaderData, total_size_of_descriptors, (void**)&memory_descriptors);
    if (EFI_ERROR(Status)) {
        printf("AllocatePool error %d\r\n", Status);
        catch_bad_status();
        return Status;
    }
    
    // Allocate memory for BootAPI regions (do this before getting map because otherwise we have to get map again to get the latest map_key)
    // @TODO Is EfiLoaderData the correct memory type? The 'regions' is an array of memory regions the kernel will keep using for ever.
    Status = ST->BootServices->AllocatePool(EfiLoaderData, total_size_of_descriptors / descriptor_size * sizeof(*regions), (void**)&regions);
    if (EFI_ERROR(Status)) {
        printf("AllocatePool error %d\r\n", Status);
        catch_bad_status();
        return Status;
    }

    // Get map again but with descriptors this time
    Status = ST->BootServices->GetMemoryMap(&total_size_of_descriptors, memory_descriptors, &map_key, &descriptor_size, &desc_version);
    if (EFI_ERROR(Status)) {
        printf("GetMemoryMap error %d\r\n", Status);
        catch_bad_status();
        return Status;
    }

    const int desc_count = total_size_of_descriptors/descriptor_size;
    // printf("Descriptors %d\r\n", desc_count);
    for (int i = 0; i < desc_count; i++) {
        EFI_MEMORY_DESCRIPTOR* desc = (EFI_MEMORY_DESCRIPTOR*)((char*)
            memory_descriptors + i*descriptor_size);
        
        if (desc->Type != EfiConventionalMemory)
            continue;

        // TODO: Do something with attributes?

        MemoryRegion* region = &regions[regions_len];
        regions_len++;

        region->physical_address = desc->PhysicalStart;
        region->page_count     = desc->NumberOfPages;
    }

    g_boot_api.regions     = regions;
    g_boot_api.regions_len = regions_len;
    g_last_map_key         = map_key;

    return EFI_SUCCESS;
}

void boot_init_frame_buffer() {
    g_boot_api.frame_buffer_base   = (uint8_t*)graphics_output->Mode->FrameBufferBase;
    g_boot_api.frame_buffer_size   = graphics_output->Mode->FrameBufferSize;
    g_boot_api.frame_buffer_width  = graphics_output->Mode->Info->HorizontalResolution;
    g_boot_api.frame_buffer_height = graphics_output->Mode->Info->VerticalResolution;
    g_boot_api.frame_buffer_pixels_per_scan_line = graphics_output->Mode->Info->PixelsPerScanLine;
}

// EFI_STATUS print_memory_map() {
//     #define IS_FREE_PAGE(N) ((N == EfiConventionalMemory) || (N == EfiPersistentMemory))
//     // We want to keep these (N >= EfiLoaderCode && N <= EfiBootServicesData)

//     int reserved_pages;
//     int free_pages;
//     for (int i = 0; i < g_memory_mapper.total_size_of_descriptors/g_memory_mapper.descriptor_size; i++) {
//         EFI_MEMORY_DESCRIPTOR* region = (EFI_MEMORY_DESCRIPTOR*)((char*)g_memory_mapper.memory_descriptors + i*g_memory_mapper.descriptor_size);
        
//         int free = IS_FREE_PAGE(region->Type);
//         if (free) {
//             free_pages += region->NumberOfPages;
//         } else {
//             reserved_pages += region->NumberOfPages;
//         }

//         printf("Region %d, phys: %d, pages: %d, virt: %d\r\n", i, (int)region->PhysicalStart, (int)region->NumberOfPages, (int)region->VirtualStart);
//         printf("  type %s, attr: %d\r\n", efi_memory_type_names[region->Type], (int)region->Attribute);
//     }
    
//     printf("Reserved pages: %d\r\n", reserved_pages);
//     printf("Free pages: %d\r\n", free_pages);
//     return EFI_SUCCESS;
// }


EFI_STATUS load_kernel_from_file(void* address);

void done_net() {

}

EFI_STATUS load_kernel(void* address) {
    EFI_STATUS status;
    
    int kernel_file_size = 4*0x100000; // 4 MB
    const int PAGE_SIZE = 4096;
    int pages = (kernel_file_size + PAGE_SIZE-1) / PAGE_SIZE;

    
    status = ST->BootServices->AllocatePages(AllocateAddress, EfiLoaderData, pages, (EFI_PHYSICAL_ADDRESS*)&address);
    if (EFI_ERROR(status)) {
        printf("Could not allocate pages at %x\r\n", address);
        catch_bad_status();
        return status;
    }
    
    if (!simple_network) {
        goto local_kernel;
    }
    
    status = simple_network->Start(simple_network);
    if (EFI_ERROR(status)) {
        printf("Cannot start network, %d\r\n", status);
        goto local_kernel;
        // return status;
    }

    status = simple_network->Initialize(simple_network, 0x0, 0x0);
    // status = simple_network->Initialize(simple_network, 0x100000, 0x100000);
    if (EFI_ERROR(status)) {
        printf("Cannot init network, %d\r\n", status);
        goto local_kernel;
        // return status;
    }
    

    NETBOOT_init();


    bool res = NETBOOT_request_file("/KERNEL.IMG", 0, kernel_file_size, address);
    if (!res) {
        goto local_kernel;
    }

    done_net();
    return EFI_SUCCESS;

local_kernel:
    status = load_kernel_from_file(address);
    if (EFI_ERROR(status)) {
        return status;
    }

    return EFI_SUCCESS;
}

EFI_STATUS load_kernel_from_file(void* address) {
    
    EFI_STATUS Status;
    EFI_FILE_PROTOCOL* volume;
    Status = simple_file_system->OpenVolume(simple_file_system, &volume);
    if (EFI_ERROR(Status)) {
        printf("OpenVolume %d\r\n", Status);
        catch_bad_status();
        return Status;
    }

    const char* kernel_path = "\\kernel.img";
    u16* kernel_wpath = tmp_path_wstring(kernel_path);

    EFI_FILE_PROTOCOL* handle;
    Status = volume->Open(volume, &handle, kernel_wpath, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(Status)) {
        printf("Open %d\r\n", Status);
        catch_bad_status();
        return Status;
    }

    char temp_buffer[sizeof(EFI_FILE_INFO) + 256];
    EFI_FILE_INFO* file_info = (EFI_FILE_INFO*)temp_buffer;
    UINTN buffer_size = sizeof(EFI_FILE_INFO) + 256;

    Status = handle->GetInfo(handle, &gEfiFileInfoGuid, &buffer_size, file_info);
    if (EFI_ERROR(Status)) {
        printf("GetInfo %d\r\n", Status);
        catch_bad_status();
        return Status;
    }
    // const int PAGE_SIZE = 4096;
    // int pages = (file_info->FileSize + PAGE_SIZE-1) / PAGE_SIZE;

    // Status = ST->BootServices->AllocatePages(AllocateAddress, EfiLoaderData, pages, (EFI_PHYSICAL_ADDRESS*)&address);
    // if (EFI_ERROR(Status)) {
    //     printf("Could not allocate pages at %x!!!\r\n", address);
    //     catch_bad_status();
    //     return Status;
    // }

    UINTN file_size = file_info->FileSize;
    Status = handle->Read(handle, &file_size, (void*)address);
    if (EFI_ERROR(Status)) {
        printf("Read %d\r\n", Status);
        catch_bad_status();
        return Status;
    }
    
    printf("First word: %x\r\n", *(int*)address);
    printf("Kernel size: %d KB\r\n", file_info->FileSize/1024);

    return EFI_SUCCESS;
}

#define FREEZE() while (1) pause();

EFI_STATUS EFIAPI
efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE * SystemTable) {
    // InitializeLib(imageHandle, systemTable); // <- what is this, why is it commented out?
    // Print(L"Hello world!\n");


    EFI_STATUS Status;
    EFI_INPUT_KEY Key;

    ST = SystemTable;
    BS = ST->BootServices;

    // Retrieve the Loaded Image Protocol using HandleProtocol
    Status = BS->HandleProtocol(ImageHandle, &gEfiLoadedImageProtocolGuid, (void **)&loaded_image);
    if (EFI_ERROR(Status)) {
        catch_bad_status();
        printf("LoadedImageProtocol failed: %d\r\n", Status);
        FREEZE();
        return Status;
    }


    // Print the actual base address of the loaded image
    printf("Image loaded at: 0x%x\r\n", (uint32_t)(uint64_t)loaded_image->ImageBase);

    // while (1);

    // Write image base and marker for GDB
    volatile uint64_t *marker_ptr = (uint64_t *)0x10000;
    volatile uint64_t *image_base_ptr = (uint64_t *)0x10008;
    *image_base_ptr = (uint64_t)loaded_image->ImageBase;  // Store ImageBase
    *marker_ptr = 0xDEADCE11;   // Set marker

    printf("Hello World\r\n"); // EFI Applications use Unicode and CRLF, a la Windows

    Status = BS->LocateProtocol(&gEfiSimpleNetworkProtocolGuid, NULL, (void**)&simple_network);
    if (EFI_ERROR(Status)) {
        printf("SimpleNetworkProtocol is not available, %d\r\n", Status);
        // We should not stop here? boot from what we have.
        // catch_bad_status();
        // FREEZE();
        // return Status;
    }

    Status = BS->HandleProtocol(loaded_image->DeviceHandle, &gEfiSimpleFileSystemProtocolGuid, (void**)&simple_file_system);
    if (EFI_ERROR(Status)) {
        printf("gEfiSimpleFileSystemProtocolGuid failed: 0x%lx\r\n", Status);
        catch_bad_status();
        FREEZE();
        return Status;
    }

    Status = BS->LocateProtocol(&gEfiGraphicsOutputProtocolGuid, NULL, (void**)&graphics_output);
    if (EFI_ERROR(Status)) {
        printf("gEfiGraphicsOutputProtocolGuid failed: 0x%lx\r\n", Status);
        catch_bad_status();
        FREEZE();
        return Status;
    }

    EFI_GUID acpi20 = ACPI_20_TABLE_GUID;
    
    for (int i=0;i<SystemTable->NumberOfTableEntries;i++) {
        EFI_CONFIGURATION_TABLE* table = &SystemTable->ConfigurationTable[i];
        if (!memcmp(&table->VendorGuid, &AcpiTableGuid, sizeof(EFI_GUID))) {
            g_boot_api.rsdp = table->VendorTable;
            printf("BOOT: Found ACPI 1.0, RSDP at %x.\r\n", g_boot_api.rsdp);
        } else if (!memcmp(&table->VendorGuid, &acpi20, sizeof(EFI_GUID))) {
            g_boot_api.rsdp = table->VendorTable;
            printf("BOOT: Found ACPI 2.0, RSDP at %x.\r\n", g_boot_api.rsdp);
            break;
        }
    }
    if (!g_boot_api.rsdp) {
        printf("BOOT: Could not find RSDP in EFI System Table.\r\n");
    }

    FN_BootAPI kernel_entry = NULL;

    void* address = (void*)0x00100000; // linker script hardcodes kernel at address 1 MB
    // @TODO We need to check if memory map contains usable pages at the address.
    Status = load_kernel(address);
    if (EFI_ERROR(Status)) {
        // Message printed in function
        catch_bad_status();
        FREEZE();
        return Status;
    }
    kernel_entry = address; // linker script for kernel (sections.ld) defines _start at beginning of kernel image.

    boot_init_frame_buffer();

    printf("UEFI - Exit boot services\r\n");

    Status = boot_init_memory();
    if (EFI_ERROR(Status)) {
        printf("boot_init_memory %d\r\n", Status);
        FREEZE();
        return Status;
    }

    // printf("Bef exit services\r\n");

    // We cannot print or do anything between GetMemoryMap (called in boot_init_memory) and ExitBootServices

    Status = ST->BootServices->ExitBootServices(ImageHandle, g_last_map_key);
    if (EFI_ERROR(Status)) {
        printf("ExitBootServices failed %d\r\n", (int)Status);
        catch_bad_status();
        FREEZE();
        return Status;
    }

    kernel_entry(g_boot_api);

    // printf("Returned to efi_main!?\r\n");
    FREEZE();

    // Shouldn't return. We're in the hands of the OS now
    return Status;
}

// EFI_STATUS list_content(EFI_FILE_PROTOCOL* dir, int depth, EFI_FILE_INFO* info);





    //   Some code to flush and read key.
    // /* Now wait for a keystroke before continuing, otherwise your
    //    message will flash off the screen before you see it.

    //    First, we need to empty the console input buffer to flush
    //    out any keystrokes entered before this point */
    // Status = ST->ConIn->Reset(ST->ConIn, FALSE);
    // if (EFI_ERROR(Status)) {
    //     catch_bad_status();
    //     return Status;
    // }

    // /* Now wait until a key becomes available.  This is a simple
    //    polling implementation.  You could try and use the WaitForKey
    //    event instead if you like */
    // // while ((Status = ST->ConIn->ReadKeyStroke(ST->ConIn, &Key)) == EFI_NOT_READY) ;
    // (void)Key;



// EFI_STATUS list_content(EFI_FILE_PROTOCOL* dir, int depth, EFI_FILE_INFO* info) {
//     EFI_STATUS Status;
//     UINTN buf_size;
//     if (!info) {
//         buf_size  = sizeof(EFI_FILE_INFO) + 256;
//         Status = ST->BootServices->AllocatePool(EfiLoaderData, buf_size, (void**)&info);
//         if(EFI_ERROR(Status)) {
//             catch_bad_status();
//             return Status;
//         }
//     }

//     char filename[256];

//     // Iterate files/dirs in folder
//     while (1) {
//         buf_size = sizeof(EFI_FILE_INFO) + 256;
//         Status = dir->Read(dir, &buf_size, info);
//         if (EFI_ERROR(Status) || buf_size == 0)
//             break;

//         if (info->FileName[0] == '.')
//             continue;

//         int i=0;
//         while(info->FileName[i]) {
//             filename[i] = (char)info->FileName[i];
//             i++;
//         }
//         filename[i] = '\0';

        
//         EFI_FILE_PROTOCOL* handle;
//         Status = dir->Open(dir, &handle, info->FileName, EFI_FILE_MODE_READ, 0);
//         if (EFI_ERROR(Status)) {
//             catch_bad_status();
//             return Status;
//         }
        
//         for(i=0;i<depth;i++) {
//             printf(" ");
//         }
//         if (info->Attribute & EFI_FILE_DIRECTORY) {
//             printf("%s\r\n", filename);
//             Status = list_content(handle, depth+1, info);
//             if(EFI_ERROR(Status))
//                 return Status;
//         } else {
//             printf("%s %d\r\n", filename, info->FileSize);
//         }
//         handle->Close(handle);
//     }
//     dir->SetPosition(dir, 0);

//     return EFI_SUCCESS;
// }
