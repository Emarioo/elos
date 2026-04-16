#pragma once

#include "elos/keyboard.h"

typedef struct KeymapEntry {
    int keycode;
    int scancode;
} KeymapEntry;

typedef struct Keymap {
    KeymapEntry scan_to_key[256 + 256];
    KeymapEntry key_to_scan[KEY_MAX];
} Keymap;

extern Keymap _default_keymap;


const char* key_name(int keycode);

int scancode_to_char(int scancode, int mod);

int scancode_to_keycode(int scancode);
