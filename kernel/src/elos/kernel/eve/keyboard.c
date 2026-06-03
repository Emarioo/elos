
#include "elos/keyboard.h"

#include "elos/kernel/eve/ps2.h"
#include "elos/kernel/eve/keys.h"

#include "elos/kernel_console.h"

#include "elos/user_event.h"


#define printf(...) KCON_printf(__VA_ARGS__)


extern const char* sv_keymap;

void KBD_init(BootAPI* boot_api) {
    ps2_init();
  
    ps2_load_keymap(sv_keymap, &_default_keymap);
}


// Keycode KBD_read_key(int* character, int* mods) {
//     // BLOCKING
//     int scancode = ps2_read_scancode();

//     *character = scancode_to_char(scancode, 0);
//     return scancode_to_keycode(scancode);
// }


// Keycode KBD_poll_key() {
//     int scancode = ps2_poll_scancode();
    
//     return scancode_to_keycode(scancode);
// }

KeyEvent keyEvents[MAX_KEY_EVENTS];
volatile u32 keyEvents_head;
volatile u32 keyEvents_tail;
int keyboard_mods;

void KBD_push_key_event(int scancode, int pressed) {
    if (scancode == 0)
        return;
    
    KeyEvent keyEvent = {
        .scancode = scancode,
        .pressed = pressed,
        .keycode = scancode_to_keycode(scancode),
    };

    switch (keyEvent.keycode) {
        case KEY_LSHIFT:
        case KEY_RSHIFT: {
            if (pressed)
                keyboard_mods |= KEY_MOD_SHIFT;
            else
                keyboard_mods &= ~KEY_MOD_SHIFT;
        } break;
        case KEY_LCTRL:
        case KEY_RCTRL: {
            if (pressed)
                keyboard_mods |= KEY_MOD_CTRL;
            else
                keyboard_mods &= ~KEY_MOD_CTRL;
        } break;
        case KEY_CAPSLOCK: {
            if (pressed)
                keyboard_mods |= KEY_MOD_CAPSLOCK;
            else
                keyboard_mods &= ~KEY_MOD_CAPSLOCK;
        } break;
        case KEY_RALT: {
            if (pressed)
                keyboard_mods |= KEY_MOD_ALT;
            else
                keyboard_mods &= ~KEY_MOD_ALT;
        } break;
    }
    // printf("key=%d scan=%x mod=%d pressed=%d\n", keyEvent.keycode, scancode, keyboard_mods, pressed);
    int chr = scancode_to_char(scancode, keyboard_mods);
    keyEvent.mods      = keyboard_mods;
    keyEvent.character = chr;

    keyEvents[keyEvents_head] = keyEvent;
    keyEvents_head = (keyEvents_head + 1) % MAX_KEY_EVENTS;

    ELOS_UserEvent event = {
        .type = ELOS_USER_EVENT_KEY,
        .id = 0,
        .key = {
            .kind = keyEvent.keycode,
            .character = chr,
            .scancode = scancode,
            .value = pressed != 0,
            .mods = keyboard_mods,
        },
    };

    EVE_push_event(&event);

}

bool KBD_poll_key_event(KeyEvent* keyEvent) {
    if (keyEvents[keyEvents_tail].scancode) {
        // Don't lock because if we get interrupt in between
        // and KBD_push_key_event is called which also locks
        // then it will deadlock.
        // Unless i'm mistaken this should be fine
        // as long as application can poll events faster than
        // you can type which is most likely the case.
        // A problem though is that two applications may read
        // the same key because we didn't increment tail.
        // We could just lock the polling.
        // We need to rethink this.
        *keyEvent = keyEvents[keyEvents_tail];
        keyEvents[keyEvents_tail].scancode = 0; // consumed
        keyEvents_tail = (keyEvents_tail + 1) % MAX_KEY_EVENTS;
        return true;
    }
    return false;
}



                    // keycode scancode
const char* sv_keymap = "1 118\n"
                        "2 18\n"
                        "3 89\n"
                        "4 20\n"
                        "5 57364\n"
                        "6 17\n"
                        "7 57361\n"
                        "8 102\n"
                        "9 13\n"
                        "10 90\n"
                        "12 57452\n"
                        "13 57456\n"
                        "14 57457\n"
                        "15 57449\n"
                        "16 57466\n"
                        "17 57469\n"
                        // "19 20\n" // Super key, can't be detected in qemu
                        "20 88\n"
                        "21 41\n"
                        "28 93\n"
                        "32 78\n"
                        "33 65\n"
                        "34 74\n"
                        "35 73\n"
                        "37 69\n"
                        "38 22\n"
                        "39 30\n"
                        "40 38\n"
                        "41 37\n"
                        "42 46\n"
                        "43 54\n"
                        "44 61\n"
                        "45 62\n"
                        "46 70\n"
                        "54 28\n"
                        "55 50\n"
                        "56 33\n"
                        "57 35\n"
                        "58 36\n"
                        "59 43\n"
                        "60 52\n"
                        "61 51\n"
                        "62 67\n"
                        "63 59\n"
                        "64 66\n"
                        "65 75\n"
                        "66 58\n"
                        "67 49\n"
                        "68 68\n"
                        "69 77\n"
                        "70 21\n"
                        "71 45\n"
                        "72 27\n"
                        "73 44\n"
                        "74 60\n"
                        "75 42\n"
                        "76 29\n"
                        "77 34\n"
                        "78 53\n"
                        "79 26\n"
                        "85 85\n"
                        "90 57451\n"
                        "91 57460\n"
                        "92 57461\n"
                        "93 57458\n"
                        "94 5\n"
                        "95 6\n"
                        "96 4\n"
                        "97 12\n"
                        "98 3\n"
                        "99 11\n"
                        "100 131\n"
                        "101 10\n"
                        "102 1\n"
                        "103 9\n"
                        "104 120\n"
                        "105 7\n";
