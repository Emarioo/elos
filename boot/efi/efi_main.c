/*
    EFI Application to load kernel into memory.
*/


#include <efi.h>
#include <efilib.h>

#include "elos/boot_api.h"
#include "elos/common/string.h"
#include "elos/common/intrinsics.h"

#include "netboot/netboot.h"
#include "elos/kernel/net/protocol.h"
#include "elos/kernel/driver/acpi.h"
#include "elos/kernel/config.h"

#include "kernel_impl.h"

/*
    Types
*/



/*
    Constants
*/


// #define debug(...) printf(__VA_ARGS__)
#define debug(...)



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

/*
    Function Definitions
*/

void catch_bad_status() {  }

void kernel_bug() { }




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
        printf("GetMemoryMap error %d\n", Status);
        catch_bad_status();
        return Status;
    }

    // Add a little extra space (spec recommends one descriptor’s worth)
    total_size_of_descriptors += descriptor_size * 2;

    // Allocate memory for descriptors    
    Status = ST->BootServices->AllocatePool(EfiLoaderData, total_size_of_descriptors, (void**)&memory_descriptors);
    if (EFI_ERROR(Status)) {
        printf("AllocatePool error %d\n", Status);
        catch_bad_status();
        return Status;
    }
    
    // Allocate memory for BootAPI regions (do this before getting map because otherwise we have to get map again to get the latest map_key)
    // @TODO Is EfiLoaderData the correct memory type? The 'regions' is an array of memory regions the kernel will keep using for ever.
    Status = ST->BootServices->AllocatePool(EfiLoaderData, total_size_of_descriptors / descriptor_size * sizeof(*regions), (void**)&regions);
    if (EFI_ERROR(Status)) {
        printf("AllocatePool error %d\n", Status);
        catch_bad_status();
        return Status;
    }

    // Get map again but with descriptors this time
    Status = ST->BootServices->GetMemoryMap(&total_size_of_descriptors, memory_descriptors, &map_key, &descriptor_size, &desc_version);
    if (EFI_ERROR(Status)) {
        printf("GetMemoryMap error %d\n", Status);
        catch_bad_status();
        return Status;
    }

    int total_pages = 0;

    const int desc_count = total_size_of_descriptors/descriptor_size;
    // printf("Descriptors %d\n", desc_count);
    for (int i = 0; i < desc_count; i++) {
        EFI_MEMORY_DESCRIPTOR* desc = (EFI_MEMORY_DESCRIPTOR*)((char*)
            memory_descriptors + i*descriptor_size);
        
        if (desc->Type != EfiConventionalMemory && desc->Type != EfiLoaderData)
            continue;

        // TODO: Do something with attributes?

        MemoryRegion* region = &regions[regions_len];
        regions_len++;

        region->physical_address = desc->PhysicalStart;
        region->page_count     = desc->NumberOfPages;
        total_pages += region->page_count;

        // printf("0x%x %d KB\n", region->physical_address, region->page_count * 4);
    }

    // printf("Total size: %d MB\n", total_pages / 0x100);

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


EFI_STATUS load_kernel_from_file(void* address);



EFI_STATUS load_kernel(void* address) {
    EFI_STATUS status;
    
    // @TODO Don't hardcode size. Or maybe that's fine?
    //    Text section is maxmium 1 MB in linker script.
    //    Data and bss cover the rest.
    int kernel_file_size = 4*0x100000; // 4 MB
    const int PAGE_SIZE = 4096;
    int pages = (kernel_file_size + PAGE_SIZE-1) / PAGE_SIZE;

    
    status = ST->BootServices->AllocatePages(AllocateAddress, EfiLoaderData, pages, (EFI_PHYSICAL_ADDRESS*)&address);
    if (EFI_ERROR(status)) {
        printf("Could not allocate pages at %x\n", address);
        catch_bad_status();
        return status;
    }
    
    if (!can_load_kernel_from_network) {
        goto local_kernel;
    }
    
    netboot_config.static_ip = kernel_config.static_ip;
    netboot_config.server_ips = kernel_config.netboot_ips;
    netboot_config.server_ips_len = kernel_config.netboot_ips_len;
    netboot_config.port = kernel_config.netboot_port;
    

    bool res;

    res = NETBOOT_init(&netboot_impl, &netboot_config);
    if (!res) {
        goto local_kernel;
    }

    res = NETBOOT_request_file("/KERNEL.IMG", 0, kernel_file_size, address);
    if (!res) {
        goto local_kernel;
    }

    return EFI_SUCCESS;

local_kernel:
    status = load_kernel_from_file(address);
    if (EFI_ERROR(status)) {
        return status;
    }

    return EFI_SUCCESS;
}

EFI_STATUS load_kernel_from_file(void* address) {
    printf("Loading Kernel from disk\n");

    EFI_STATUS Status;
    EFI_FILE_PROTOCOL* volume;
    Status = simple_file_system->OpenVolume(simple_file_system, &volume);
    if (EFI_ERROR(Status)) {
        printf("OpenVolume %d\n", Status);
        catch_bad_status();
        return Status;
    }

    const char* kernel_path = "\\kernel.img";
    u16* kernel_wpath = tmp_path_wstring(kernel_path);

    EFI_FILE_PROTOCOL* handle;
    Status = volume->Open(volume, &handle, kernel_wpath, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(Status)) {
        printf("Open %d\n", Status);
        catch_bad_status();
        return Status;
    }

    char temp_buffer[sizeof(EFI_FILE_INFO) + 256];
    EFI_FILE_INFO* file_info = (EFI_FILE_INFO*)temp_buffer;
    UINTN buffer_size = sizeof(EFI_FILE_INFO) + 256;

    Status = handle->GetInfo(handle, &gEfiFileInfoGuid, &buffer_size, file_info);
    if (EFI_ERROR(Status)) {
        printf("GetInfo %d\n", Status);
        catch_bad_status();
        return Status;
    }

    UINTN file_size = file_info->FileSize;
    Status = handle->Read(handle, &file_size, (void*)address);
    if (EFI_ERROR(Status)) {
        printf("Read %d\n", Status);
        catch_bad_status();
        return Status;
    }

    // @TODO CLose handles.
    
    // printf("First word: %x\n", *(int*)address);
    // printf("Kernel size: %d KB\n", file_info->FileSize/1024);

    return EFI_SUCCESS;
}

EFI_STATUS load_initrd_from_file();

EFI_STATUS load_initrd() {
    EFI_STATUS status;
    
    // @TODO Don't hardcode size. Or maybe that's fine?
    //    Text section is maxmium 1 MB in linker script.
    //    Data and bss cover the rest.
    int initrd_file_size = 8*0x100000; // 4 MB
    const int PAGE_SIZE = 4096;
    int pages = (initrd_file_size + PAGE_SIZE-1) / PAGE_SIZE;

    void* address = NULL;
    
    status = ST->BootServices->AllocatePages(AllocateAnyPages, EfiLoaderData, pages, (EFI_PHYSICAL_ADDRESS*)&address);
    if (EFI_ERROR(status)) {
        printf("Could not allocate pages at %x\n", address);
        catch_bad_status();
        return status;
    }
    
    if (!can_load_kernel_from_network) {
        goto local_kernel;
    }
    
    netboot_config.static_ip = kernel_config.static_ip;
    netboot_config.server_ips = kernel_config.netboot_ips;
    netboot_config.server_ips_len = kernel_config.netboot_ips_len;
    netboot_config.port = kernel_config.netboot_port;
    

    bool res;

    res = NETBOOT_init(&netboot_impl, &netboot_config);
    if (!res) {
        goto local_kernel;
    }

    int file_size = NETBOOT_request_file("/INITRD.IMG", 0, initrd_file_size, address);
    if (!file_size) {
        goto local_kernel;
    }
    
    g_boot_api.initrd      = (void*)address;
    g_boot_api.initrd_size = file_size;

    return EFI_SUCCESS;

local_kernel:
    status = load_initrd_from_file();
    if (EFI_ERROR(status)) {
        return status;
    }

    return EFI_SUCCESS;
}

EFI_STATUS load_initrd_from_file() {
    printf("Loading initrd from disk\n");

    EFI_STATUS Status;
    EFI_FILE_PROTOCOL* volume;
    Status = simple_file_system->OpenVolume(simple_file_system, &volume);
    if (EFI_ERROR(Status)) {
        printf("OpenVolume %d\n", Status);
        catch_bad_status();
        return Status;
    }

    const char* file_path = "\\INITRD.IMG";
    u16* wpath = tmp_path_wstring(file_path);

    EFI_FILE_PROTOCOL* handle;
    Status = volume->Open(volume, &handle, wpath, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(Status)) {
        printf("Open %d\n", Status);
        catch_bad_status();
        return Status;
    }

    char temp_buffer[sizeof(EFI_FILE_INFO) + 256];
    EFI_FILE_INFO* file_info = (EFI_FILE_INFO*)temp_buffer;
    UINTN buffer_size = sizeof(EFI_FILE_INFO) + 256;

    Status = handle->GetInfo(handle, &gEfiFileInfoGuid, &buffer_size, file_info);
    if (EFI_ERROR(Status)) {
        printf("GetInfo %d\n", Status);
        catch_bad_status();
        return Status;
    }

    UINTN file_size = file_info->FileSize;

    EFI_PHYSICAL_ADDRESS initrd_data;
    Status = ST->BootServices->AllocatePages(AllocateAnyPages, EfiLoaderData, (file_size + EFI_PAGE_SIZE-1)/EFI_PAGE_SIZE, &initrd_data); 
    if (EFI_ERROR(Status)) {
        printf("initrd AllocatePages err=%d size=%d MB\n", Status, file_size/0x100000);
        catch_bad_status();
        return Status;
    }

    Status = handle->Read(handle, &file_size, (void*)initrd_data);
    if (EFI_ERROR(Status)) {
        printf("Read %d\n", Status);
        catch_bad_status();
        return Status;
    }

    // @TODO CLose handles.

    g_boot_api.initrd      = (void*)initrd_data;
    g_boot_api.initrd_size = file_size;
    
    // printf("First word: %x\n", *(int*)address);
    // printf("Kernel size: %d KB\n", file_info->FileSize/1024);

    return EFI_SUCCESS;
}

EFI_STATUS read_kernel_config() {
    
    EFI_STATUS Status;
    EFI_FILE_PROTOCOL* volume;
    Status = simple_file_system->OpenVolume(simple_file_system, &volume);
    if (EFI_ERROR(Status)) {
        printf("OpenVolume %d\n", Status);
        catch_bad_status();
        return Status;
    }

    const char* kernel_path = "\\TEMPLATE.CFG";
    u16* kernel_wpath = tmp_path_wstring(kernel_path);

    EFI_FILE_PROTOCOL* handle;
    Status = volume->Open(volume, &handle, kernel_wpath, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(Status)) {
        printf("Open %d\n", Status);
        catch_bad_status();
        return Status;
    }

    char temp_buffer[sizeof(EFI_FILE_INFO) + 256];
    EFI_FILE_INFO* file_info = (EFI_FILE_INFO*)temp_buffer;
    UINTN buffer_size = sizeof(EFI_FILE_INFO) + 256;

    Status = handle->GetInfo(handle, &gEfiFileInfoGuid, &buffer_size, file_info);
    if (EFI_ERROR(Status)) {
        printf("GetInfo %d\n", Status);
        catch_bad_status();
        return Status;
    }

    void* text;
    Status = ST->BootServices->AllocatePool(AllocateAnyPages, file_info->FileSize, &text);
    if (EFI_ERROR(Status)) {
        printf("AllocatePool, read config, %d\n", Status);
        catch_bad_status();
        return Status;
    }

    UINTN file_size = file_info->FileSize;
    Status = handle->Read(handle, &file_size, text);
    if (EFI_ERROR(Status)) {
        printf("Read %d\n", Status);
        catch_bad_status();
        return Status;
    }

    char* error;
    bool parsed = CFG_parse(text, file_size, &kernel_config, &error);
    if (!parsed) {
        printf("KernelConfig parser error: %s\n", error);
        return -1;
    }

    char buffer0[30];
    printf("static_ip = %s\n", ipv4_str((u8*)&kernel_config.static_ip, buffer0));
    printf("netboot_port = %d\n", kernel_config.netboot_port);
    printf("netboot_ips[0] = %s\n", ipv4_str((u8*)&kernel_config.netboot_ips[0], buffer0));
    printf("netboot_ips[1] = %s\n", ipv4_str((u8*)&kernel_config.netboot_ips[1], buffer0));

    return EFI_SUCCESS;
}



#define FREEZE() while (1) pause();

// Breakpoint when debugging
volatile void efi_entry() { }

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
        printf("LoadedImageProtocol failed: %d\n", Status);
        FREEZE();
        return Status;
    }


    // Print the actual base address of the loaded image
    printf("Image loaded at: 0x%x\n", (uint32_t)(uint64_t)loaded_image->ImageBase);

    // while (1);


    // Write image base and marker for GDB
    volatile uint64_t *marker_ptr = (uint64_t *)0x10000;
    volatile uint64_t *image_base_ptr = (uint64_t *)0x10008;
    *image_base_ptr = (uint64_t)loaded_image->ImageBase;  // Store ImageBase
    *marker_ptr = 0xDEADCE11;   // Set marker

    printf("Hello World\n"); // EFI Applications use Unicode and CRLF, a la Windows


    efi_entry();


    Status = BS->HandleProtocol(loaded_image->DeviceHandle, &gEfiSimpleFileSystemProtocolGuid, (void**)&simple_file_system);
    if (EFI_ERROR(Status)) {
        printf("gEfiSimpleFileSystemProtocolGuid failed: 0x%lx\n", Status);
        catch_bad_status();
        FREEZE();
        return Status;
    }

    Status = BS->LocateProtocol(&gEfiGraphicsOutputProtocolGuid, NULL, (void**)&graphics_output);
    if (EFI_ERROR(Status)) {
        printf("gEfiGraphicsOutputProtocolGuid failed: 0x%lx\n", Status);
        catch_bad_status();
        FREEZE();
        return Status;
    }

    Status = read_kernel_config();
    if (EFI_ERROR(Status)) {
        // Error already printed.
        catch_bad_status();
        FREEZE();
        return Status;
    }

    // Comment out to disable network boot.
    // init_network();
    // Or set this:
    // can_load_kernel_from_network = false;


    EFI_GUID acpi20 = ACPI_20_TABLE_GUID;
    
    for (int i=0;i<SystemTable->NumberOfTableEntries;i++) {
        EFI_CONFIGURATION_TABLE* table = &SystemTable->ConfigurationTable[i];
        if (!memcmp(&table->VendorGuid, &AcpiTableGuid, sizeof(EFI_GUID))) {
            RSDP* rsdp = table->VendorTable;
            // We extract physical address from RSDP because pointer to RSDP is virtual address
            // and we need physical address when setting up our own page tables.
            g_boot_api.rsdt = (void*)(u64)rsdp->RsdtAddress;
            printf("BOOT: Found ACPI 1.0, RSDP at %x.\n", rsdp);
        } else if (!memcmp(&table->VendorGuid, &acpi20, sizeof(EFI_GUID))) {
            XSDP* xsdp = table->VendorTable;
            g_boot_api.rsdt = (void*)xsdp->XsdtAddress;
            printf("BOOT: Found ACPI 2.0, XSDP at %x.\n", xsdp);
            break;
        }
    }
    if (!g_boot_api.rsdt) {
        printf("BOOT: Could not find RSDP in EFI System Table.\n");
    }

    FN_BootAPI kernel_entry = NULL;

    void* kernel_address = __KERNEL_START;

    // @TODO We need to check if memory map contains usable pages at the address.
    Status = load_kernel(kernel_address);
    if (EFI_ERROR(Status)) {
        // Message printed in function
        catch_bad_status();
        FREEZE();
        return Status;
    }
    kernel_entry = kernel_address; // linker script for kernel (sections.ld) defines _start at beginning of kernel image.

    load_initrd();

    boot_init_frame_buffer();

    EFI_PHYSICAL_ADDRESS stack = (u64)__STACK_START;
    int stack_size = (u64)__STACK_END - (u64)__STACK_START;
    // int stack_size = 10000;

    Status = ST->BootServices->AllocatePages(AllocateAddress, EfiLoaderData, stack_size / EFI_PAGE_SIZE, &stack);
    if (EFI_ERROR(Status)) {
        // Message printed in function
        printf("Could not allocate stack at 0x%x (size %d KB), %d", stack, stack_size / 1024, Status);
        catch_bad_status();
        FREEZE();
        return Status;
    }

    // FREEZE();

    printf("UEFI - Exit boot services\n");

    // FREEZE();

    // Sleep if we want.
    // EFI_EVENT timer_event;
    // UINTN index;
    // Status = ST->BootServices->CreateEvent(EFI_EVENT_TIMER, TPL_APPLICATION,NULL,NULL,&timer_event);
    // if (EFI_ERROR(Status)) {
    //     printf("CreateEvent failed, %d\n", Status);
    //     return 1;
    // }
    // Status = ST->BootServices->RaiseTPL(TPL_APPLICATION);
    // if (EFI_ERROR(Status)) {
    //     printf("RaiseTPL failed, %d\n", Status);
    //     return 1;
    // }
    // Status = ST->BootServices->SetTimer(timer_event, TimerRelative, 10*1000LL*1000LL*10); // 100ns units
    // if (EFI_ERROR(Status)) {
    //     printf("SetTimer failed, %d\n", Status);
    //     return 1;
    // }
    // Status = ST->BootServices->WaitForEvent(1, &timer_event, &index);
    // if (EFI_ERROR(Status)) {
    //     printf("WaitForEvent %d\n", Status);
    //     FREEZE();
    //     return Status;
    // }


    Status = boot_init_memory();
    if (EFI_ERROR(Status)) {
        printf("boot_init_memory %d\n", Status);
        FREEZE();
        return Status;
    }

    // We cannot print or do anything between GetMemoryMap (called in boot_init_memory) and ExitBootServices

    Status = ST->BootServices->ExitBootServices(ImageHandle, g_last_map_key);
    if (EFI_ERROR(Status)) {
        printf("ExitBootServices failed %d\n", (int)Status);
        catch_bad_status();
        FREEZE();
        return Status;
    }

    kernel_entry(g_boot_api);

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
//             printf("%s\n", filename);
//             Status = list_content(handle, depth+1, info);
//             if(EFI_ERROR(Status))
//                 return Status;
//         } else {
//             printf("%s %d\n", filename, info->FileSize);
//         }
//         handle->Close(handle);
//     }
//     dir->SetPosition(dir, 0);

//     return EFI_SUCCESS;
// }
