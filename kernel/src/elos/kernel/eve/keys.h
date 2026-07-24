#pragma once

#include "elos/keyboard.h"

typedef struct Keymap {
    ELOS_Keycode scancode_to_keycode[512];
    u32          scancode_to_char[512][4];
} Keymap;



const char* key_name(ELOS_Keycode keycode);

ELOS_Keycode scancode_to_keycode(Keymap* keymap, u32 scancode);
u32 scancode_to_character(Keymap* keymap, u32 scancode, u32 mod, ELOS_Keycode* keycode);
