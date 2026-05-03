
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

#include "elos/kernel/video/frame.h"

#include "elos/kernel/net/protocol.h"

bool load_font();



extern FN_KCON_write _write_hooks[4];

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
        // We are in serious trouble without frame buffer.
    }

    // Initialize physical memory component with:
    //  - Memory regions we can access and how much RAM we have.
    //  - Virtual Paging.

    KCON_printf("Initializing physical memory regions\n");
    PMEM_init(boot_api);

    // draw_rect(200, 400, 50,50, GREEN);

    // Initialize simple keyboard
    KBD_init(boot_api);

    // draw_rect(200, 500, 50,50, GREEN);

    // Initialize interrupt tables
    CPU_init(boot_api);

    // draw_rect(200, 600, 50,50, GREEN);

    // while (1) ;

    // @TODO Initialize file system and disk devices

    NetDevice net_device = NULL;
    int count = 1;
    NET_scan_devices(&net_device, &count);
    // @TODO Check that we got device

    
    NET_set_receive_callback(net_device, handle_packet, NULL);

    // u32 target_ip = ipv4_from_str("192.168.100.50");
    // NET_send_arp(net_device, target_ip); // QEMU

    // target_ip = ipv4_from_str("192.168.0.60");
    // NET_send_arp(net_device, target_ip); // On laptop

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
    KCON_printf("KERNEL BUG");
    bugs++;
}
