
#include "elos/boot_api.h"

#include "elos/kernel/video/font/font.h"

#include "elos/keyboard.h"
#include "elos/kernel_console.h"
#include "elos/network.h"
#include "elos/frame_buffer.h"
#include "elos/physical_memory.h"
#include "elos/cpu.h"
#include "elos/disk.h"
#include "elos/execution.h"
#include "elos/vfs.h"

#include "elos/common/intrinsics.h"
#include "elos/common/string.h"

#include "elos/kernel/net/i8254x.h"

#include "elos/kernel/video/frame.h"

#include "elos/kernel/net/protocol.h"

#include "elos/kernel/pmem/paging.h"

#define printf(...) KCON_printf(__VA_ARGS__)


bool load_font();

void terminal_main();

extern FN_KCON_write _write_hooks[4];

void handle_packet(NetDevice device, NET_Packet* packet, void* user_data);

BootAPI _boot_api;
BootAPI* boot_api;
void kernel_entry(BootAPI* in_boot_api) {
    CPU_enable_sse();

    // Put BootAPI in kernel's memory space. in_boot_api will become invalid when
    // we setup our own page tables.
    _boot_api = *in_boot_api;
    boot_api = &_boot_api;
    
    

    // Have .bss section in kernel image been memory mapped?
    // In theory the .data, .rodata, .text should be mapped because we allocate memory for kernel image from
    // EFI. EFI sets up page tables for the kernel image. In our kernel entry does
    // page tables still exist.
    // When we setup or own page tables we reuse the ones EFI setup.
    // .bss section is not included in the kernel image and should therefore not have page tables set up.
    // Perhaps EFI happens to setup page tables for surrounding memory and in practise .bss happens to be mapped correctly too.

    // Initialize kernel console.
    // Kernel components logs information to it.
    // serial UART is default output.
    KCON_init(boot_api);

    if (boot_api->frame_buffer_base) {
        FB_init(boot_api);

        // Write over UEFI prints
        draw_rect(0, 0, 400, 400, DARK_BLUE);

        // If frame buffer is available initialize it.
        // KCON_printf("Initializing frame buffer\n");
        // Kernel components will now write to serial UART and
        // frame buffer whenever a Kernel Component logs anything.
        // (we see stuff on the display as well as a log file from QEMU)

        bool loaded_font = load_font();
        if (!loaded_font) {
            draw_rect(400, 400, 25,25, RED);
            // We are in serious trouble here.
        } else {
            KCON_add_write_hook(FB_write);
            KCON_printf("Loaded default font\n");
        }
    } else {
        // No display.
    }

    // Initialize physical memory component with:
    //  - Memory regions we can access and how much RAM we have.
    //  - Virtual Paging.

    KCON_printf("Initializing physical memory regions\n");
    PMEM_init(boot_api);

    // Initialize simple keyboard
    KBD_init(boot_api);

    // Initialize GDT, IDT, interrupt tables, vectors
    // Timer interrupt, parse ACPI tables for IOAPIC, APIC, HPET
    CPU_init(boot_api);


    DiskDevice diskDevices[8];
    int diskDevices_len = ARRAY_LENGTH(diskDevices);

    DISK_scan_devices(diskDevices, &diskDevices_len);
    KCON_printf("Disk devices: %d\n", diskDevices_len);

    for (int i=0;i<diskDevices_len;i++) {
        DiskDevice dev = diskDevices[i];
        DiskInfo diskInfo = {0};
        DISK_get_info(dev, &diskInfo);

        char path[256];
        snprintf(path, sizeof(path), "/dev%dp%d", i, 0);

        bool res = VFS_mount(path, dev, 0);
        if (res) {
            KCON_printf("Mounted %s (%d MB) at %s\n", diskInfo.name, diskInfo.diskSize/1024/1024, path);
        } else {
            KCON_printf("Could not mount %s at %s\n", diskInfo.name, path);
        }
    }

    Texture* texture = load_texture("/dev0p0/back1.png");
    if (texture) {
        int width, height;
        draw_frame_info(&width,&height);
        draw_texture(0, 0, width, height, 0, 0, texture->width, texture->height, texture);
    } else {
        printf("No texture\n");
    }

    printf("Rendered\n");

    // const char* path = "/dev0p0/hello.txt";
    // VFS_Handle handle = VFS_open(path, VFS_FLAG_READ);
    // if (handle == VFS_NULL_HANDLE) {
    //     printf("Could not open %s\n", path);
    // } else {
    //     VFS_HandleInfo info = {0};
    //     VFS_info(handle, &info);
    //     printf("%s: %d bytes\n", path, info.fileSize);

    //     char buffer[512];
    //     u64 readBytes = VFS_read(handle, 0, 512, buffer);

    //     printf("Content (%d bytes):\n", readBytes);
    //     for(int i=0;i<readBytes;i++) {
    //         if (buffer[i] == '\0') // @TODO Currently VFS_read reads all bytes of the sector even if DirectoryEntry file size isn't that big.
    //             break;
    //         printf("%c", buffer[i]);
    //     }
    //     printf("\n");
    // }

    // if (diskDevices_len > 0) {
    //     char buffer[512];

    //     DISK_read(diskDevices[0], 0, 512, buffer);
        
    //     KCON_printf("Data at sector 0: ");
    //     for (int i=0;i<16;i++) {
    //         KCON_printf("%c", buffer[i]);
    //     }
    //     KCON_printf("\n");

    //     memcpy(buffer + 5, "----", 4);
        
    //     DISK_write(diskDevices[0], 0, 512, buffer);

    //     memset(buffer, 0, 512);
        
    //     DISK_read(diskDevices[0], 0, 512, buffer);
        
    //     KCON_printf("Data at sector 0: ");
    //     for (int i=0;i<16;i++) {
    //         KCON_printf("%c", buffer[i]);
    //     }
    //     KCON_printf("\n");

    // }

    



    // NetDevice net_device = NULL;
    // int count = 1;
    // NET_scan_devices(&net_device, &count);
    // @TODO Check that we got device

    
    // NET_set_receive_callback(net_device, handle_packet, NULL);

    // u32 target_ip = ipv4_from_str("192.168.100.50");

    // In QEMU you need to setup DHCP server or use "-netdev user,id=net0"
    // NET_send_dhcp_discover(net_device);
    
    // In QEMU you cannot use "-netdev user,id=net0".
    // You must setup tap device (which is preferably anyway because you can use wireshark)
    // NET_send_arp(net_device, target_ip);



    // KCON_printf("END OF KERNEL_ENTRY!\n");

    // EXEC_init();
    // EXEC_create_thread(terminal_main, 0);
    

    while (1) {
        // if (count) {
        //     NET_Packet packet;
        //     bool yes = NET_poll_packet(net_device, &packet);
        //     if (yes) {
        //         NET_handle_packet(net_device, &packet);
        //         NET_free_packet(net_device, &packet);
        //     }
        // }

        pause();
    }

}

void handle_packet(NetDevice device, NET_Packet* packet, void* user_data) {
    NET_handle_packet(device, packet);
}


// These are defined in kernel Makefile (created from res/Lat2-Terminus16.psf at the time of writing this)
extern u8 __default_font_start;
extern u8 __default_font_end;

static u8 _default_font_glyph_data[0x20000];

void* _default_font_allocator(Allocator* allocator, u64 size, void* old_ptr) {
    if (size > sizeof(_default_font_glyph_data)) {
        return NULL;
    }
    return _default_font_glyph_data;
}

bool load_font() {
    Allocator allocator = { _default_font_allocator };
    bool res = font__load_from_bytes(&__default_font_start, &__default_font_end - &__default_font_start, &g_default_font, &allocator);
    return res;
}


int bugs;
void kernel_bug() {
    KCON_printf("KERNEL BUG\n");
    bugs++;
}


/*

Code to test paging

    KCON_printf("Before crash\n");
    void* phys_ptr = PMEM_alloc_phys(PAGE_SIZE, PMEM_FLAG_NONE);
    int* v1_ptr = (void*)0xa000000;
    int* v2_ptr = (void*)0xc001000;
    bool yes = PMEM_map_memory(v1_ptr, phys_ptr, PAGE_SIZE);
    if (!yes) {
        KCON_printf("BAD\n");
    }
    yes = PMEM_map_memory(v2_ptr, phys_ptr, PAGE_SIZE);
    if (!yes) {
        KCON_printf("BAD2\n");
    }
    v1_ptr[0] = 99;
    int value = v2_ptr[0];
    KCON_printf("After crash %d\n", value);
*/
