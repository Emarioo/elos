
#include "elos/keyboard.h"

#include "elos/kernel/eve/ps2.h"
#include "elos/kernel/eve/keys.h"

#include "elos/kernel_console.h"
#include "elos/common/string.h"
#include "elos/common/intrinsics.h"

#include "elos/user_event.h"
#include "elos/system_console.h"
#include "elos/cpu.h"


#define printf(...) KCON_printf(__VA_ARGS__)


Keymap* g_currentKeymap;

void KBD_init(BootAPI* boot_api) {
    ps2_init();
  
    KBD_set_layout("sv");
}

#define MAX_ELOSKEY_EVENTS 256

// Unfortunately timing in QEMU differs from hardware.
// Values below are nice in QEMU. Might be too slow/fast for real hardware.
u64 repeatDelay_ms = 280;
u64 repeatInterval_ms = 60;
bool useSoftwareRepeat = true;

typedef struct {
    u32  scancode;
    u64  nextRepeatTick;
} KeyState;

bool heldKeyTable[512];

KeyState keyStates[64];
u64      keyStates_len;

KeyEvent keyEvents[MAX_ELOSKEY_EVENTS];
volatile u32 keyEvents_head;
volatile u32 keyEvents_tail;
u32 keyboard_mods;

static void push_key_event(int scancode, int pressed);

void KBD_push_key_event(int scancode, int pressed) {
    if (scancode >= sizeof(heldKeyTable)) {
        // HUH?
        return;
    }
    if (useSoftwareRepeat) {
        if (heldKeyTable[scancode] == (pressed != 0)) {
            // Ignore repeat event since we implement it in software.
            return;
        }
        if (pressed != 0) {
            KeyState* state = &keyStates[keyStates_len];
            state->scancode = scancode;
            state->nextRepeatTick = rdtsc() + repeatDelay_ms * (CPU_tsc_per_sec()/1000);
            keyStates_len++;
        } else {
            for (int i=0;i<keyStates_len;i++) {
                KeyState* state = &keyStates[i];
                if (state->scancode == scancode) {
                    memcpy(&keyStates[i], &keyStates[i+1], sizeof(KeyState) * (keyStates_len - i - 1));
                    keyStates_len--;
                    break;
                }
            }
        }
    }
    
    // If keyboard is unplugged we should clear all held keys.
    // and keystates.
    heldKeyTable[scancode] = pressed != 0;

    push_key_event(scancode, pressed);
}
static void push_key_event(int scancode, int pressed) {
    if (scancode == 0)
        return;
    
    KeyEvent keyEvent = {
        .scancode = scancode,
        .pressed = pressed,
        .keycode = scancode_to_keycode(g_currentKeymap, scancode),
    };

    switch ((int)keyEvent.keycode) {
        case ELOSKEY_LEFT_SHIFT:
        case ELOSKEY_RIGHT_SHIFT: {
            if (pressed)
                keyboard_mods |= ELOSKEY_MOD_SHIFT;
            else
                keyboard_mods &= ~ELOSKEY_MOD_SHIFT;
        } break;
        case ELOSKEY_LEFT_CTRL:
        case ELOSKEY_RIGHT_CTRL: {
            if (pressed)
                keyboard_mods |= ELOSKEY_MOD_CTRL;
            else
                keyboard_mods &= ~ELOSKEY_MOD_CTRL;
        } break;
        case ELOSKEY_CAPS_LOCK: {
            if (pressed) {
                keyboard_mods ^= ELOSKEY_MOD_CAPS_LOCK;
            }
        } break;
        case ELOSKEY_RIGHT_ALT: {
            if (pressed) {
                keyboard_mods |= ELOSKEY_MOD_ALT;
            } else {
                keyboard_mods &= ~ELOSKEY_MOD_ALT;
            }
        } break;
        case ELOSKEY_LEFT_SUPER:
        case ELOSKEY_RIGHT_SUPER: {
            if (pressed) {
                keyboard_mods |= ELOSKEY_MOD_SUPER;
            } else {
                keyboard_mods &= ~ELOSKEY_MOD_SUPER;
            }
        } break;
        case ELOSKEY_NUM_LOCK: {
            if (pressed) {
                keyboard_mods ^= ELOSKEY_MOD_NUM_LOCK;
            }
        } break;
    }
    // printf("key=%d scan=%x mod=%d pressed=%d\n", keyEvent.keycode, scancode, keyboard_mods, pressed);
    u32 chr = scancode_to_character(g_currentKeymap, scancode, keyboard_mods, NULL);
    keyEvent.mods      = keyboard_mods;
    keyEvent.character = chr;

    keyEvents[keyEvents_head] = keyEvent;
    keyEvents_head = (keyEvents_head + 1) % MAX_ELOSKEY_EVENTS;

    if (SCON_is_enabled()) {
        // System console is layered above all other applications.
        // Keys are therefore only sent to it.
        return;
    }

    ELOS_UserEvent event = {
        .type = ELOS_USER_EVENT_KEY,
        .id = 0, // ps2 driver only supports one device at the moment
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
        keyEvents_tail = (keyEvents_tail + 1) % MAX_ELOSKEY_EVENTS;
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


void KBD_tick_handler() {
    u64 nowTick = rdtsc();

    for (int i=0;i<keyStates_len;i++) {
        KeyState* state = &keyStates[i];

        // int presses = 0;
        while (nowTick > state->nextRepeatTick) {
            push_key_event(state->scancode, 1);
            state->nextRepeatTick += repeatInterval_ms * (CPU_tsc_per_sec()/1000);
            // pressed++;
        }
        // if (presses) {
        //     push_key_event(state->scancode, presses);
        // }
    }

}
