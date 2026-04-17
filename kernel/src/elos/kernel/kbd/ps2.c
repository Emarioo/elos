#include "elos/kernel/kbd/ps2.h"

#include "elos/common/intrinsics.h"
#include "elos/kernel_console.h"
#include "elos/common/cpu.h"
#include "elos/common/types.h"
#include "elos/common/string.h"
#include "elos/kernel/kbd/keys.h"



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
        __pause();
        int data = ps2_read_byte();
        // printf("Got %d\n", data);
        if (data == 0xE0) {
            int data2 = ps2_read_byte();
            if (data2 == 0xF0) {
                // Release don't care
                data = ps2_read_byte();
            } else {
                return 0xE000 | data2;
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

    // These bits keep second PS/2 port bits and disables interrupt
    // for first PS/2, sets system flag, enabled first port clock
    // @TODO Set second PS/2 port clock, if enabled
    config_byte = (config_byte & 0b00100010) | 0b00000100;
    ps2_send_write_command(0x60, config_byte);

    printf("ps2: Set config byte %d\n", (int)config_byte);

    // Self test controller
    byte = ps2_send_read_command(0xAA);
    if (byte != 0x55) {
        // PS/2 not available?
        return 1;
    }
    // Restore configuration byte, self test may have affected it
    ps2_send_write_command(0x60, config_byte);

    ps2_send_command(0xA8);

    config_byte = ps2_send_read_command(0x20);
    if (config_byte & 0b10000) {
        // PS2 is disabled
        printf("second PS2 disabled\n");
    } else {
        // PS2 is enable
        printf("second PS2 enabled, disabling...\n");

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
    ps2_send_command(0xFF);
    printf("ps2: Ports are reset\n");
    // sleep_ns(500000000);


    ps2_write_byte(0xF0);
    ps2_write_byte(2);
    printf("ps2: Pick scancode set\n");
    // sleep_ns(500000000);

    while (1) {
        int val = inb(KBD_STATUS);
        if ((val & PS2_READ_STATUS_MASK)) {
            val = inb(KBD_DATA);
            printf("Expected ACK: %x (0xFA)\n", val);
            break;
        }
    }

    ps2_write_byte(0xF4);
    printf("ps2: Enable scanning\n");
    // sleep_ns(500000000);

    while (1) {
        int val = inb(KBD_STATUS);
        if ((val & PS2_READ_STATUS_MASK)) {
            val = inb(KBD_DATA);
            printf("Expected ACK: %x (0xFA)\n", val);
            break;
        }
    }


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


static int _keycode_list[] = {
    KEY_ESCAPE,
    KEY_F1,
    KEY_F2,
    KEY_F3,
    KEY_F4,
    KEY_F5,
    KEY_F6,
    KEY_F7,
    KEY_F8,
    KEY_F9,
    KEY_F10,
    KEY_F11,
    KEY_F12,
    KEY_1,
    KEY_2,
    KEY_3,
    KEY_4,
    KEY_5,
    KEY_6,
    KEY_7,
    KEY_8,
    KEY_9,
    KEY_0,
    KEY_PLUS,
    KEY_BACKTICK,
    KEY_BACKSPACE,
    KEY_TAB,
    KEY_Q,
    KEY_W,
    KEY_E,
    KEY_R,
    KEY_T,
    KEY_Y,
    KEY_U,
    KEY_I,
    KEY_O,
    KEY_P,
    KEY_ENTER,
    KEY_CAPSLOCK,
    KEY_A,
    KEY_S,
    KEY_D,
    KEY_F,
    KEY_G,
    KEY_H,
    KEY_J,
    KEY_K,
    KEY_L,
    KEY_SQUOTE,
    KEY_LSHIFT,
    KEY_Z,
    KEY_X,
    KEY_C,
    KEY_V,
    KEY_B,
    KEY_N,
    KEY_M,
    KEY_COMMA,
    KEY_PERIOD,
    KEY_MINUS,
    KEY_RSHIFT,
    KEY_LCTRL,
    // KEY_SUPER,
    KEY_LALT,
    KEY_SPACE,
    KEY_RALT,
    KEY_RCTRL,
    KEY_UP_ARROW,
    KEY_LEFT_ARROW,
    KEY_DOWN_ARROW,
    KEY_RIGHT_ARROW,
};

Keymap _default_keymap;

int ps2_ask_keymap() {
    
    int key_index = 0;
    int key_len = sizeof(_keycode_list)/sizeof(*_keycode_list);

    while (key_index < key_len) {
        int keycode = _keycode_list[key_index];
        key_index++;

        const char* name = key_name(keycode);

        printf("Press: %s ", name);

        int scancode = ps2_read_scancode();
        printf("%x\n", scancode);

        _default_keymap.key_to_scan[keycode].keycode = keycode; 
        _default_keymap.key_to_scan[keycode].scancode = scancode;
        _default_keymap.scan_to_key[(((scancode >> 16) == 0xE0) ? 256 : 0) + (scancode & 0xFF)].keycode = keycode; 
        _default_keymap.scan_to_key[(((scancode >> 16) == 0xE0) ? 256 : 0) + (scancode & 0xFF)].scancode = scancode;
    }

    // serial_printf("Keymap (keycode,scancode):\n");
    // for (int i=0;i<KEY_MAX;i++) {
    //     KeymapEntry* entry = &_default_keymap.key_to_scan[i];
    //     if (entry->keycode != KEY_NONE) {
    //         serial_printf("%d %d\n", entry->keycode, entry->scancode);
    //     }
    // }

    return 0;
}


int parse_int(const char* text, int* _head, int* out) {
    int head = *_head;
    
    int acc = 0;
    while (1) {
        char chr = text[head];
        if (chr >= '0' && chr <= '9') {
            acc = acc * 10 + chr - '0';
            head++;
            continue;
        }
        if (head == *_head)
            return 1;
        break;
    }
    *out = acc;
    *_head = head;
    return 0;
}

int ps2_load_keymap(const char* text, Keymap* keymap) {
    int res;
    int head = 0;
    int len = strlen(text);

    int keycode,scancode;

    while (head < len) {
        while (text[head] == ' ') head++;

        // parse integer
        res = parse_int(text, &head, &keycode);
        if (res) return res;

        while (text[head] == ' ') head++;

        res = parse_int(text, &head, &scancode);
        if (res) return res;

        while (text[head] == ' ') head++;

        if (text[head] != '\n')
            return 1;
        head++;

        keymap->key_to_scan[keycode].keycode = keycode;
        keymap->key_to_scan[keycode].scancode = scancode;
        keymap->scan_to_key[(((scancode >> 16) == 0xE0) ? 256 : 0) + (scancode & 0xFF)].keycode = keycode;
        keymap->scan_to_key[(((scancode >> 16) == 0xE0) ? 256 : 0) + (scancode & 0xFF)].scancode = scancode;
    }

    return 0;
}
