#pragma once


#include "elos/kernel/common/types.h"
#include "elos/kernel/driver/keys.h"

int ps2_init();

int ps2_read_scancode();

int ps2_ask_keymap();

int ps2_load_keymap(const char* text, Keymap* keymap);
