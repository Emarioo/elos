
#include "elos/boot_api.h"

#include "elos/kernel/video/font/font.h"

#include "elos/keyboard.h"
#include "elos/kernel_console.h"
#include "elos/network.h"
#include "elos/frame_buffer.h"
#include "elos/physical_memory.h"
#include "elos/cpu.h"

#include "elos/common/intrinsics.h"

#include "elos/kernel/net/i8254x.h"

void load_font();


void handle_packet(NetDevice device, NET_Packet* packet, void* user_data);

void kernel_entry(BootAPI* boot_api) {

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

    // Initialize physical memory component with:
    //  - Memory regions we can access and how much RAM we have.
    //  - Virtual Paging.
    KCON_printf("Initializing physical memory regions\n");
    PMEM_init(boot_api);

    load_font();

    if (boot_api->frame_buffer_base) {
        
        // If frame buffer is available initialize it.
        KCON_printf("Initializing frame buffer\n");
        FB_init(boot_api);
        KCON_add_write_hook(FB_write);
        // Kernel components will now write to serial UART and
        // frame buffer whenever a Kernel Component logs anything.
        // (we see stuff on the display as well as a log file from QEMU)
    }

    // Initialize simple keyboard
    KBD_init(boot_api);

    // Initialize interrupt tables
    CPU_init(boot_api);

    // @TODO Initialize file system and disk devices

    NetDevice net_device = NULL;
    int count = 1;
    NET_scan_devices(&net_device, &count);
    // @TODO Check that we got device

    
    NET_set_receive_callback(net_device, handle_packet, NULL);

    KCON_printf("END OF KERNEL_ENTRY!\n");
    while (1) {
        if (count) {
            NET_Packet packet;
            bool yes = NET_poll_packet(net_device, &packet);
            if (yes) {
                NET_handle_packet(net_device, &packet);
                NET_free_packet(net_device, &packet);
            }
        }
        pause();
    }

}

void handle_packet(NetDevice device, NET_Packet* packet, void* user_data) {
    NET_handle_packet(device, packet);
}


// These are defined in kernel Makefile (created from res/Lat2-Terminus16.psf at the time of writing this)
extern u8 __default_font_start;
extern u8 __default_font_end;

void load_font() {
    bool res = font__load_from_bytes(&__default_font_start, &__default_font_end - &__default_font_start, &g_default_font);
    if (!res) {
        KCON_printf("Could not load default font\n");
    } else {
        KCON_printf("Loaded default font\n");
    }
}


int bugs;
void kernel_bug() {
    KCON_printf("KERNEL BUG");
    bugs++;
}
