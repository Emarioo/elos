/*
    EFI Application to load kernel into memory.
*/


#include <efi.h>
#include <efilib.h>

#include "elos/boot_api.h"
#include "elos/common/string.h"
#include "elos/common/intrinsics.h"

#include "netboot/netboot.h"
#include "elos/network.h"
#include "elos/kernel/net/protocol.h"

/*
    Types
*/


// structure for revision 0 (version 1.0)
#pragma pack(push, 1)
typedef struct RSDP {
    char Signature[8];
    u8   Checksum;
    char OEMID[6];
    u8   Revision;
    u32  RsdtAddress;
} RSDP;
#pragma pack(pop)

// structure for revision 2 (version 2.0+)
#pragma pack(push, 1)
typedef struct XSDP {
 char Signature[8];
 u8   Checksum;
 char OEMID[6];
 u8   Revision;
 u32  RsdtAddress;      // deprecated since version 2.0

 u32  Length;
 u64  XsdtAddress;
 u8   ExtendedChecksum;
 u8   reserved[3];
} XSDP;
#pragma pack(pop)

/*
    Constants
*/


// #define debug(...) printf(__VA_ARGS__)
#define debug(...)

// linker script hardcodes kernel at address 2 MB
#define __kernel_start ((void*)0x200000)
#define __kernel_end   ((void*)(0x200000 + 0x200000))
#define __stack_start ((void*)0x1800000)
#define __stack_end   ((void*)(0x1800000 + 0x200000))


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


bool can_load_kernel_from_network;
EFI_SIMPLE_NETWORK_PROTOCOL* simple_network;

NetBoot_Impl netboot_impl;
NetBoot_Config netboot_config;
NetBoot_Device netboot_device;

/*
    Function Declarations
*/

EFI_STATUS boot_init_memory();

void boot_init_frame_buffer();

EFI_STATUS load_kernel();

int efi_recv(NetBoot_Device device, void* buffer, int* out_size);
int efi_send(NetBoot_Device device, void* buffer, int size);

int netdrv_recv(NetBoot_Device device, void* buffer, int* out_size);
int netdrv_send(NetBoot_Device device, void* buffer, int size);


/*
    Function Definitions
*/

void catch_bad_status() {  }

void kernel_bug() { }

void printf(const char* format, ...) {
    char buffer[256];
    unsigned short w_buffer[256];

    va_list va;
    va_start(va, format);
    const int len = vsnprintf(buffer, sizeof(buffer), format, va);
    va_end(va);

    int head=0;
    int dst_head=0;
    while(head < len + 1) {
        if (buffer[head] == '\n' && (head-1 < 0 || buffer[head-1] != '\r')) {
            w_buffer[dst_head] = '\r';
            dst_head++;
        }
        w_buffer[dst_head] = buffer[head];
        head++;
        dst_head++;
    }
    EFI_STATUS status = ST->ConOut->OutputString(ST->ConOut, w_buffer);
    (void)status;
}


void KCON_printf(const char* format, ...) {
    char buffer[256];
    unsigned short w_buffer[256];

    va_list va;
    va_start(va, format);
    const int len = vsnprintf(buffer, sizeof(buffer), format, va);
    va_end(va);

    int head=0;
    int dst_head=0;
    while(head < len + 1) {
        if (buffer[head] == '\n' && (head-1 < 0 || buffer[head-1] != '\r')) {
            w_buffer[dst_head] = '\r';
            dst_head++;
        }
        w_buffer[dst_head] = buffer[head];
        head++;
        dst_head++;
    }
    EFI_STATUS status = ST->ConOut->OutputString(ST->ConOut, w_buffer);
    (void)status;
}

void* PMEM_allocate(u64 size, void* old_ptr) {
    EFI_STATUS Status;
    if (size && !old_ptr) {
        void* addr = 0;
        Status = ST->BootServices->AllocatePool(EfiLoaderData, (8 + size), &addr);
        if (EFI_ERROR(Status)) {
            printf("Pool failed, %d, %d bytes\n", Status, size);
            return NULL;
        }
        // *(int*)addr = size;
        // return (char*)addr + 8;
        return (char*)addr;
    } else if(!size) {
        // void* real_addr = (char*)old_ptr - 8;
        void* real_addr = (char*)old_ptr;
        Status = ST->BootServices->FreePool(real_addr);
        if (EFI_ERROR(Status)) {
            printf("Pool free failed, %d, %d bytes\n", Status);
            return NULL;
        }
        return NULL;
    } else {
        // @TODO Implement?
        printf("NO REALLOCATE\n");
        kernel_bug();
        return NULL;
        // void* real_addr = (char*)old_ptr - 8;
        // int old_size = *(int*)real_addr;
        // void* addr = 0;
        // Status = ST->BootServices->AllocatePool(EfiLoaderData, (8 + size + EFI_PAGE_SIZE-1)/EFI_PAGE_SIZE, &addr);
        // if (EFI_ERROR(Status)) {
        //     printf("Pool failed, %d, %d bytes\n", Status, size);
        //     return NULL;
        // }
        // *(int*)addr = size;
        // memcpy((char*)addr + 8, old_ptr, old_size);
        // Status = ST->BootServices->FreePool(real_addr);
        // return (char*)addr + 8;
    }
}

void* PMEM_alloc_phys(u64 size, int flags) {
    EFI_STATUS Status;
    EFI_PHYSICAL_ADDRESS addr = 0;
    Status = ST->BootServices->AllocatePages(AllocateAnyPages, EfiLoaderData, (size+EFI_PAGE_SIZE-1)/EFI_PAGE_SIZE, &addr);
    if (EFI_ERROR(Status)) {
        printf("Pool failed, %d, %d bytes\n", Status, size);
        return NULL;
    }
    return (void*)addr;
}

void PMEM_map_memory(void* vaddr, void* paddr, uint64_t size) {
    if (vaddr != paddr) {
        printf("Only identitiy mapping allowed, %x -> %x (%d bytes)\n", paddr, vaddr, size);
        return;
    }
    EFI_STATUS Status;
    EFI_PHYSICAL_ADDRESS addr = (EFI_PHYSICAL_ADDRESS)paddr;
    Status = ST->BootServices->AllocatePages(AllocateAddress, EfiLoaderData, (size+EFI_PAGE_SIZE-1)/EFI_PAGE_SIZE, &addr);
    if (EFI_ERROR(Status)) {
        printf("Could not map %x -> %x (%d bytes), %d\n", paddr, vaddr, size, Status);
    }
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
    static u32 ips[2];
    
    netboot_config.server_ips = ips;
    netboot_config.server_ips_len = ARRAY_LENGTH(ips);
    // The IP address for tap0 must have it's own subnet or it can intefere with the subnet the Host is connected to.
    // Consequence is no packets sent or received (doesn't show up in wireshark)
    ips[0] = ipv4_from_str("192.168.100.50"); // QEMU tap0
    ips[1] = ipv4_from_str("192.168.0.60");   // BEAST computer

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
    
    printf("First word: %x\n", *(int*)address);
    printf("Kernel size: %d KB\n", file_info->FileSize/1024);

    return EFI_SUCCESS;
}

#define FREEZE() while (1) pause();

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


    
    // Status = BS->LocateProtocol(&gEfiSimpleNetworkProtocolGuid, NULL, (void**)&simple_network);
    // if (EFI_ERROR(Status)) {
    //     printf("SimpleNetworkProtocol is not available, %d\n", Status);
    // } else {
    //     Status = simple_network->Start(simple_network);
    //     if (EFI_ERROR(Status)) {
    //         printf("Cannot start network, %d\n", Status);
    //     } else {
    //         Status = simple_network->Initialize(simple_network, 0x0, 0x0);
    //         // status = simple_network->Initialize(simple_network, 0x100000, 0x100000);
    //         if (EFI_ERROR(Status)) {
    //             printf("Cannot init network, %d\n", Status);
    //         } else {
    //             netboot_impl.recv = efi_recv;
    //             netboot_impl.send = efi_send;
    //             memcpy(netboot_impl.mac, simple_network->Mode->CurrentAddress.Addr, 6);
    //             netboot_device = NULL;
    //             can_load_kernel_from_network = true;
    //         }
    //     }
    // }

    if (!can_load_kernel_from_network) {
            
        NetDevice devices[6];
        int devices_len = ARRAY_LENGTH(devices);
        NET_scan_devices(devices, &devices_len);

        if (devices_len == 0) {
            printf("Non-UEFI network driver could not find supported controller (no rtl8169)\n");
        } else {
            NetDevice device = devices[0];
            NET_DeviceInfo devinfo;
            NET_device_info(device, &devinfo);

            netboot_impl.recv = netdrv_recv;
            netboot_impl.send = netdrv_send;
            memcpy(netboot_impl.mac, devinfo.mac, 6);
            netboot_device = device;
            can_load_kernel_from_network = true;
        }
    }


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

    void* kernel_address = __kernel_start;

    // @TODO We need to check if memory map contains usable pages at the address.
    Status = load_kernel(kernel_address);
    if (EFI_ERROR(Status)) {
        // Message printed in function
        catch_bad_status();
        FREEZE();
        return Status;
    }
    kernel_entry = kernel_address; // linker script for kernel (sections.ld) defines _start at beginning of kernel image.

    boot_init_frame_buffer();

    EFI_PHYSICAL_ADDRESS stack = (u64)__stack_start;
    int stack_size = (u64)__stack_end - (u64)__stack_start;
    // int stack_size = 10000;

    Status = ST->BootServices->AllocatePages(AllocateAddress, EfiLoaderData, stack_size / EFI_PAGE_SIZE, &stack);
    if (EFI_ERROR(Status)) {
        // Message printed in function
        printf("Could not allocate stack at 0x%x (size %d KB), %d", stack, stack_size / 1024, Status);
        catch_bad_status();
        FREEZE();
        return Status;
    }


    printf("UEFI - Exit boot services\n");

    // FREEZE();

    EFI_EVENT timer_event;
    UINTN index;

    Status = ST->BootServices->CreateEvent(EFI_EVENT_TIMER, TPL_APPLICATION,NULL,NULL,&timer_event);
    if (EFI_ERROR(Status)) {
        printf("CreateEvent failed, %d\n", Status);
        return 1;
    }
    Status = ST->BootServices->RaiseTPL(TPL_APPLICATION);
    if (EFI_ERROR(Status)) {
        printf("RaiseTPL failed, %d\n", Status);
        return 1;
    }
    
    Status = ST->BootServices->SetTimer(timer_event, TimerRelative, 10*1000LL*1000LL*10); // 100ns units
    if (EFI_ERROR(Status)) {
        printf("SetTimer failed, %d\n", Status);
        return 1;
    }

    Status = ST->BootServices->WaitForEvent(1, &timer_event, &index);
    if (EFI_ERROR(Status)) {
        printf("WaitForEvent %d\n", Status);
        FREEZE();
        return Status;
    }


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


int efi_recv(NetBoot_Device device, void* buffer, int* out_size) {
    
    EFI_STATUS status;
    UINTN buffer_size = *out_size;
    UINT32 interruptMask;
    void* tx_buf;
    
    status = simple_network->GetStatus(simple_network, &interruptMask, &tx_buf);

    status = simple_network->Receive(simple_network, 0, &buffer_size, buffer, NULL, NULL, NULL);
    if (status == EFI_NOT_READY) {
        // printf("RECV NOT ready\r\n");
        *out_size = 0;
        return 0;
    }
    if (EFI_ERROR(status)) {
        printf("Cannot receive network, %d, buffer size %d\r\n", status, buffer_size);
        *out_size = 0;
        return 0;
    }
    status = simple_network->GetStatus(simple_network, &interruptMask, &tx_buf);

    EFI_NETWORK_STATISTICS stats = {0};
    UINTN stat_size = sizeof(EFI_NETWORK_STATISTICS);
    status = simple_network->Statistics(simple_network, 0, &stat_size, &stats);
    if (EFI_ERROR(status)) {
        printf("Statistics error, %d\r\n", status);
    }

    debug("Got %d bytes, good=%d total=%x drop=%x totalb=%d statS=%d\n", buffer_size, stats.RxGoodFrames, stats.RxTotalFrames, stats.RxDroppedFrames, stats.RxTotalBytes, stat_size);

    *out_size = buffer_size;
    return buffer_size;
}
int efi_send(NetBoot_Device device, void* buffer, int size) {
    EFI_STATUS status;
    
    status = simple_network->Transmit(simple_network, 0, size, (void*)buffer, NULL, NULL, NULL);
    if (EFI_ERROR(status)) {
        printf("Could not transmit, %d\r\n", status);
        return 0;
    }

    UINT32 interruptMask;
    void* tx_buf;
    while (1) {
        status = simple_network->GetStatus(simple_network, &interruptMask, &tx_buf);
        pause();
        if (tx_buf == buffer)
            break;
    }
    return size;
}

int netdrv_recv(NetBoot_Device device, void* buffer, int* out_size) {
    NET_Packet packet;
    bool yes = NET_poll_packet(device, &packet);
    if (!yes) {
        *out_size = 0;
        return 0;
    }
    int bytes = *out_size >= packet.size ? packet.size : *out_size;
    memcpy(buffer, packet.buffer, bytes);
    NET_free_packet(device, &packet);
    return bytes;
}
int netdrv_send(NetBoot_Device device, void* buffer, int size) {
    NET_send_packet(device, buffer, size);
    return size;
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
