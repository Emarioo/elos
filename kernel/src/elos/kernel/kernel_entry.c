
#include "elos/boot_api.h"

#include "elos/keyboard.h"
#include "elos/kernel_console.h"
#include "elos/frame_buffer.h"
#include "elos/physical_memory.h"
#include "elos/cpu.h"


void kernel_entry(BootAPI* boot_api) {

    // Initialize kernel console.
    // Kernel components logs information to it.
    // serial UART is default output.
    KCON_init(boot_api);

    if (boot_api->frame_buffer_base) {
        
        // If frame buffer is available initialize it.
        KCON_printf("Initializing frame buffer");
        FB_init(boot_api);
        KCON_add_write_hook(FB_write);
        // Kernel components will now write to serial UART and
        // frame buffer whenever a Kernel Component logs anything.
        // (we see stuff on the display as well as a log file from QEMU)
    }

    // Initialize simple keyboard
    KBD_init(boot_api);

    // Initialize physical memory component with:
    //  - Memory regions we can access and how much RAM we have.
    //  - Virtual Paging.
    KCON_printf("Initializing physical memory regions");
    PMEM_init(boot_api);

    // Initialize interrupt tables
    CPU_init(boot_api);

    // Initialize file system, or disk devices?


    KCON_printf("END OF KERNEL_ENTRY!");
    while (1) ;

}


int bugs;
void kernel_bug() {
    KCON_printf("KERNEL BUG");
    bugs++;
}
