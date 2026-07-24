
#include "elos/keyboard.h"

#include "elos/kernel/eve/ps2.h"
#include "elos/kernel/eve/keys.h"

#include "elos/kernel_console.h"
#include "elos/common/string.h"

#include "elos/user_event.h"


#define printf(...) KCON_printf(__VA_ARGS__)


Keymap* g_currentKeymap;

void KBD_init(BootAPI* boot_api) {
    ps2_init();
  
    KBD_set_layout("sv");
}

#define MAX_KEY_EVENTS 256

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
        .keycode = scancode_to_keycode(g_currentKeymap, scancode),
    };

    switch ((int)keyEvent.keycode) {
        case KEY_LEFT_SHIFT:
        case KEY_RIGHT_SHIFT: {
            if (pressed)
                keyboard_mods |= KEY_MOD_SHIFT;
            else
                keyboard_mods &= ~KEY_MOD_SHIFT;
        } break;
        case KEY_LEFT_CTRL:
        case KEY_RIGHT_CTRL: {
            if (pressed)
                keyboard_mods |= KEY_MOD_CTRL;
            else
                keyboard_mods &= ~KEY_MOD_CTRL;
        } break;
        case KEY_CAPS_LOCK: {
            if (pressed)
                keyboard_mods ^= KEY_MOD_CAPS_LOCK;
        } break;
        case KEY_RIGHT_ALT: {
            if (pressed)
                keyboard_mods |= KEY_MOD_ALT;
            else
                keyboard_mods &= ~KEY_MOD_ALT;
        } break;
        case KEY_SUPER: {
            if (pressed)
                keyboard_mods |= KEY_MOD_SUPER;
            else
                keyboard_mods &= ~KEY_MOD_SUPER;
        } break;
        case KEY_NUM_LOCK: {
            if (pressed)
                keyboard_mods ^= KEY_MOD_NUM_LOCK;
        } break;
    }
    // printf("key=%d scan=%x mod=%d pressed=%d\n", keyEvent.keycode, scancode, keyboard_mods, pressed);
    u32 chr = scancode_to_character(g_currentKeymap, scancode, keyboard_mods, NULL);
    keyEvent.mods      = keyboard_mods;
    keyEvent.character = chr;

    keyEvents[keyEvents_head] = keyEvent;
    keyEvents_head = (keyEvents_head + 1) % MAX_KEY_EVENTS;

    ELOS_UserEvent event = {
        .type = ELOS_USER_EVENT_KEY,
        .id = 0,
        .key = {
            .keycode = keyEvent.keycode,
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

extern Keymap sv_keymap;

bool KBD_set_layout(const char* layout) {
    if (!strcmp(layout, "sv")) {
        g_currentKeymap = &sv_keymap;
        return true;
    } else {
        // If layout is a path then read and parse it?
    }
    return false;
}

