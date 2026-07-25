#pragma once

#include "elos/boot_api.h"

#include "elos/common/types.h"
#include "elos/keycode.h"


void KBD_init(BootAPI* boot_api);


typedef struct {
    u32 scancode;
    ELOS_Keycode keycode;
    u32 character;
    u32 mods;
    u32 pressed;
} KeyEvent;


bool KBD_set_layout(const char* layout);

void KBD_tick_handler();
void KBD_push_key_event(int scancode, int pressed);
bool KBD_poll_key_event(KeyEvent* keyEvent);
