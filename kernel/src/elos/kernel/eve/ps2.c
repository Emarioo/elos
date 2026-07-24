#include "elos/kernel/eve/ps2.h"

#include "elos/common/intrinsics.h"
#include "elos/kernel_console.h"
#include "elos/cpu.h"
#include "elos/common/types.h"
#include "elos/common/string.h"
#include "elos/kernel/eve/keys.h"



#define KBD_DATA 0x60
#define KBD_STATUS 0x64


#define PS2_READ_STATUS_MASK 0x1
#define PS2_WRITE_STATUS_MASK 0x2
#define PS2_TIME_OUT_MASK 0x40
#define PS2_PARITY_MASK 0x80


#define printf(...) KCON_printf(__VA_ARGS__)

extern int us_keymap[256];
extern int sv_keymap[256];

u8 ps2_status() {
    return inb(KBD_STATUS);
}

u8 ps2_send_read_command(u8 cmd) {
    while ((inb(KBD_STATUS) & PS2_WRITE_STATUS_MASK)) ;
    outb(KBD_STATUS, cmd);
    while ((inb(KBD_STATUS) & PS2_READ_STATUS_MASK) == 0) ;
    return inb(KBD_DATA);
}

int ps2_read_byte() {
    while (1) {
        int res = inb(KBD_STATUS);
        if ((res & PS2_READ_STATUS_MASK)) {
            break;
        }
    }
    return inb(KBD_DATA);
}
void ps2_write_byte(u8 byte) {
    while (1) {
        int res = inb(KBD_STATUS);
        if ((res & PS2_WRITE_STATUS_MASK) == 0) {
            break;
        }
    }
    outb(KBD_DATA, byte);
}

int ps2_read_scancode() {
    while (1) {
        pause();
        int data = ps2_read_byte();
        // printf("Got %d\n", data);
        if (data == 0xE0) {
            int data2 = ps2_read_byte();
            if (data2 == 0xF0) {
                // Release don't care
                data = ps2_read_byte();
            } else {
                return 0x100 | data2;
            }
        } else if (data == 0xF0) {
            // Key release, don't care about data
            data = ps2_read_byte();
        } else {
            // key press, we want the code
            return data;
        }
    }
}


int ps2_poll_scancode(int* pressed) {
    
    int res = inb(KBD_STATUS);
    if ((res & PS2_READ_STATUS_MASK) == 0) {
        // no key
        return 0;
    }

    int data = inb(KBD_DATA);
    int scancode = 0;

    if (data == 0xE0) {
        scancode |= 0x100;
        data = ps2_read_byte();
    }
    if (data == 0xF0) {
        data = ps2_read_byte();
        *pressed = 0;
    } else {
        *pressed = 1;
    }
    scancode |= data;
    return scancode;
}


void ps2_send_write_command(u8 cmd, u8 byte) {
    // @TODO Status register has "Command/data" where
    //      0 = data written to input buffer is data for PS/2 device
    //      1 = data written to input buffer is data for PS/2 controller command
    //    Do i need to worry about this?
    while ((inb(KBD_STATUS) & PS2_WRITE_STATUS_MASK)) ;
    outb(KBD_STATUS, cmd);
    while ((inb(KBD_STATUS) & PS2_WRITE_STATUS_MASK)) ;
    outb(KBD_DATA, byte);
}
void ps2_send_command(u8 cmd) {
    while ((inb(KBD_STATUS) & PS2_WRITE_STATUS_MASK)) ;
    outb(KBD_STATUS, cmd);
}
void ps2_enable_interrupts(bool enabled) {
    ps2_send_command(0xAD);
    ps2_send_command(0xA7);
    if (enabled) {
        u8 config_byte = ps2_send_read_command(0x20);
        config_byte = (config_byte & 0b01110100) | 1;
        ps2_send_write_command(0x60, config_byte);
    } else {
        u8 config_byte = ps2_send_read_command(0x20);
        config_byte = (config_byte & 0b01110100);
        ps2_send_write_command(0x60, config_byte);
    }

    ps2_send_command(0xAE);
}

int ps2_init() {
    u8 byte;
    u8 config_byte;
    // @TODO Check ACPI table for PS/2 controller

    while ((inb(KBD_STATUS) & PS2_WRITE_STATUS_MASK)) ;
    outb(KBD_STATUS, 0xAD); // Disable first PS/2 port
    while ((inb(KBD_STATUS) & PS2_WRITE_STATUS_MASK)) ;
    outb(KBD_STATUS, 0xA7); // Disable second PS/2 port (if it exists)

    // Flush buffer in case of leftover bytes
    // We disabled PS/2 ports so no new bytes will be sent
    while ((inb(KBD_STATUS) & PS2_READ_STATUS_MASK)) {
        (void) inb(KBD_DATA);
    }

    config_byte = ps2_send_read_command(0x20);

    bool enable_interrupt = true;

    // These bits keep second PS/2 port bits and disables interrupt
    // for first PS/2, sets system flag, enabled first port clock
    // @TODO Set second PS/2 port clock, if enabled
    config_byte = (config_byte & 0b00100010) | 0b00000100;
    if (enable_interrupt) {
        config_byte |= 0b1;
    }
    ps2_send_write_command(0x60, config_byte);

    // printf("ps2: Set config byte %d\n", (int)config_byte);

    // Self test controller
    byte = ps2_send_read_command(0xAA);
    if (byte != 0x55) {
        // PS/2 not available?
        return 1;
    }
    // Restore configuration byte, self test may have affected it
    ps2_send_write_command(0x60, config_byte);

    // ps2_send_command(0xA8);

    config_byte = ps2_send_read_command(0x20);
    if (config_byte & 0b10000) {
        // PS2 is disabled
        // printf("second PS2 disabled\n");
    } else {
        // PS2 is enable
        // printf("second PS2 enabled, disabling...\n");

        ps2_send_command(0xA7);
        config_byte = ps2_send_read_command(0x20);
        config_byte = (config_byte & 0b01010101) | 0b00000100;
        ps2_send_write_command(0x60, config_byte);
    }

    // Test first port
    byte = ps2_send_read_command(0xAB);
    if (byte != 0) {
        // Test failed
        return 1;
    }
    // Enable first PS/2 port
    ps2_send_command(0xAE);

    // Reset ports
    int limit = 500;
    while ((inb(KBD_STATUS) & PS2_WRITE_STATUS_MASK) == 0 && limit > 0) {
        limit--;
    };
    outb(KBD_DATA, 0xFF);
    // printf("ps2: Ports are reset\n");
    // sleep_ns(500000000);


    ps2_write_byte(0xF0);
    ps2_write_byte(2);
    // printf("ps2: Pick scancode set\n");
    // sleep_ns(500000000);

    while (1) {
        int val = inb(KBD_STATUS);
        if ((val & PS2_READ_STATUS_MASK)) {
            val = inb(KBD_DATA);
            // printf("Expected ACK: %x (0xFA)\n", val);
            break;
        }
    }

    ps2_write_byte(0xF4);
    // printf("ps2: Enable scanning\n");
    // sleep_ns(500000000);

    while (1) {
        int val = inb(KBD_STATUS);
        if ((val & PS2_READ_STATUS_MASK)) {
            val = inb(KBD_DATA);
            // printf("Expected ACK: %x (0xFA)\n", val);
            break;
        }
    }

    // config_byte = ps2_send_read_command(0x20);
    // printf("config byte: %d\n", config_byte);


    // Let's print bytes we receive from ps2

    // while (1) {
    //     int res = inb(KBD_STATUS);
        
    //     if ((res & PS2_READ_STATUS_MASK)) {
    //         int resd = inb(KBD_DATA);
    //         printf("Status %x, %x\n", res, resd);
    //     }
        
    //     sleep_ns(5000000);
    // }

    // int byte0 = 0;
    // int byte1 = 0;
    // while (1) {
    //     int res = inb(KBD_STATUS);
    //     if ((res & PS2_READ_STATUS_MASK) != 0) {
    //         byte0 = inb(KBD_DATA);
    //         break;
    //     }
    // }
    // while (1) {
    //     int res = inb(KBD_STATUS);
    //     if ((res & PS2_READ_STATUS_MASK) != 0) {
    //         byte1 = inb(KBD_DATA);
    //         break;
    //     }
    // }

    // printf("Bytes %d %d\n", byte0, byte1);

    return 0;
}

