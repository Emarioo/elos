/*

    Some UEFI implementations do not provide network protocols.
    Since we want to boot from the network during development we
    have to implement our own. The kernel already implements a network driver
    so we will use that. To use it we must implement a couple of 
    kernel functions below. They map to EFI requivalent functions.

*/


#include <efi.h>
#include <efilib.h>

#include "kernel_impl.h"

#include "elos/common/string.h"
#include "elos/common/intrinsics.h"


#include "netboot/netboot.h"
#include "elos/network.h"
// #include "elos/kernel/net/protocol.h"


int efi_recv(NetBoot_Device device, void* buffer, int* out_size);
int efi_send(NetBoot_Device device, void* buffer, int size);

int netdrv_recv(NetBoot_Device device, void* buffer, int* out_size);
int netdrv_send(NetBoot_Device device, void* buffer, int size);

bool can_load_kernel_from_network;
EFI_SIMPLE_NETWORK_PROTOCOL* simple_network;

// These don't belong to "kernel implementation"
NetBoot_Impl netboot_impl;
NetBoot_Config netboot_config;
NetBoot_Device netboot_device;

KernelConfig kernel_config;

typedef void PageTable;

PageTable* g_kernelPageTable;

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



void init_network() {
    EFI_STATUS Status;
    Status = BS->LocateProtocol(&gEfiSimpleNetworkProtocolGuid, NULL, (void**)&simple_network);
    if (EFI_ERROR(Status)) {
        printf("SimpleNetworkProtocol is not available, %d\n", Status);
    } else {
        Status = simple_network->Start(simple_network);
        if (EFI_ERROR(Status)) {
            printf("Cannot start network, %d\n", Status);
        } else {
            Status = simple_network->Initialize(simple_network, 0x0, 0x0);
            // status = simple_network->Initialize(simple_network, 0x100000, 0x100000);
            if (EFI_ERROR(Status)) {
                printf("Cannot init network, %d\n", Status);
            } else {
                netboot_impl.recv = efi_recv;
                netboot_impl.send = efi_send;
                memcpy(netboot_impl.mac, simple_network->Mode->CurrentAddress.Addr, 6);
                netboot_device = NULL;
                can_load_kernel_from_network = true;
            }
        }
    }

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
}


void CPU_sleep(u64 nanoseconds) {
    static u64 rdtsc_base;
    if (!rdtsc_base)
        rdtsc_base = rdtsc();

    // @TODO seconds to cycles conversion
    u64 target = nanoseconds + (rdtsc() - rdtsc_base);

    while(1) {
        u64 now = rdtsc() - rdtsc_base;
        if (now >= target)
            break;
        pause();
    }
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

    // printf("Got %d bytes, good=%d total=%x drop=%x totalb=%d statS=%d\n", buffer_size, stats.RxGoodFrames, stats.RxTotalFrames, stats.RxDroppedFrames, stats.RxTotalBytes, stat_size);

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


