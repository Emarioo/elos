
#include "elos/boot_api.h"

#include "elos/kernel/video/font/font.h"

#include "elos/keyboard.h"
#include "elos/kernel_console.h"
#include "elos/frame_buffer.h"
#include "elos/physical_memory.h"
#include "elos/cpu.h"

void load_font();

void kernel_entry(BootAPI* boot_api) {

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

    // Initialize file system, or disk devices?
    

    KCON_printf("END OF KERNEL_ENTRY!\n");
    while (1) ;

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
