#pragma once

#include "elos/kernel/eve/keys.h"

typedef enum PS2_ScanCode {
    /* Number row */

    PS2_SC_1               = 0x0016,
    PS2_SC_2               = 0x001E,
    PS2_SC_3               = 0x0026,
    PS2_SC_4               = 0x0025,
    PS2_SC_5               = 0x002E,
    PS2_SC_6               = 0x0036,
    PS2_SC_7               = 0x003D,
    PS2_SC_8               = 0x003E,
    PS2_SC_9               = 0x0046,
    PS2_SC_0               = 0x0045,


    /* Alphabet */

    PS2_SC_A               = 0x001C,
    PS2_SC_W               = 0x001D,
    PS2_SC_C               = 0x0021,
    PS2_SC_D               = 0x0023,
    PS2_SC_B               = 0x0032,
    PS2_SC_E               = 0x0024,
    PS2_SC_F               = 0x002B,
    PS2_SC_G               = 0x0034,
    PS2_SC_H               = 0x0033,
    PS2_SC_I               = 0x0043,
    PS2_SC_J               = 0x003B,
    PS2_SC_K               = 0x0042,
    PS2_SC_L               = 0x004B,
    PS2_SC_M               = 0x003A,
    PS2_SC_N               = 0x0031,
    PS2_SC_O               = 0x0044,
    PS2_SC_P               = 0x004D,
    PS2_SC_Q               = 0x0015,
    PS2_SC_R               = 0x002D,
    PS2_SC_S               = 0x001B,
    PS2_SC_T               = 0x002C,
    PS2_SC_Y               = 0x0035,
    PS2_SC_U               = 0x003C,
    PS2_SC_Z               = 0x001A,
    PS2_SC_X               = 0x0022,
    PS2_SC_V               = 0x002A,


    /* Symbols */

    PS2_SC_GRAVE           = 0x000E,
    PS2_SC_MINUS           = 0x004E,
    PS2_SC_EQUAL           = 0x0055,

    PS2_SC_LEFT_BRACKET    = 0x0054,
    PS2_SC_RIGHT_BRACKET   = 0x005B,
    PS2_SC_BACKSLASH       = 0x005D,

    PS2_SC_SEMICOLON       = 0x004C,
    PS2_SC_APOSTROPHE      = 0x0052,

    PS2_SC_COMMA           = 0x0041,
    PS2_SC_PERIOD          = 0x0049,
    PS2_SC_SLASH           = 0x004A,


    /* Control / modifiers */

    PS2_SC_ESCAPE          = 0x0076,
    PS2_SC_TAB             = 0x000D,

    PS2_SC_CAPS_LOCK       = 0x0058,

    PS2_SC_LEFT_SHIFT      = 0x0012,
    PS2_SC_RIGHT_SHIFT     = 0x0059,

    PS2_SC_LEFT_CTRL       = 0x0014,
    PS2_SC_LEFT_ALT        = 0x0011,

    PS2_SC_SPACE           = 0x0029,
    PS2_SC_ENTER           = 0x005A,
    PS2_SC_BACKSPACE       = 0x0066,
    PS2_SC_ARROW           = 0x0061, // swedish keyboard key next to shift and Z


    /* Keypad */

    PS2_SC_NUM_LOCK        = 0x0077,

    PS2_SC_KP_DIVIDE       = 0x014A,
    PS2_SC_KP_MULTIPLY     = 0x007C,
    PS2_SC_KP_MINUS        = 0x007B,
    PS2_SC_KP_PLUS         = 0x0079,

    PS2_SC_KP_7            = 0x006C,
    PS2_SC_KP_8            = 0x0075,
    PS2_SC_KP_9            = 0x007D,

    PS2_SC_KP_4            = 0x006B,
    PS2_SC_KP_5            = 0x0073,
    PS2_SC_KP_6            = 0x0074,

    PS2_SC_KP_1            = 0x0069,
    PS2_SC_KP_2            = 0x0072,
    PS2_SC_KP_3            = 0x007A,

    PS2_SC_KP_0            = 0x0070,
    PS2_SC_KP_PERIOD       = 0x0071,

    PS2_SC_KP_ENTER        = 0x015A,


    /* Function keys */

    PS2_SC_F1              = 0x0005,
    PS2_SC_F2              = 0x0006,
    PS2_SC_F3              = 0x0004,
    PS2_SC_F4              = 0x000C,
    PS2_SC_F5              = 0x0003,
    PS2_SC_F6              = 0x000B,
    PS2_SC_F7              = 0x0083,
    PS2_SC_F8              = 0x000A,
    PS2_SC_F9              = 0x0001,
    PS2_SC_F10             = 0x0009,
    PS2_SC_F11             = 0x0078,
    PS2_SC_F12             = 0x0007,


    /* Navigation */

    PS2_SC_INSERT          = 0x0170,
    PS2_SC_DELETE          = 0x0171,
    PS2_SC_HOME            = 0x016C,
    PS2_SC_END             = 0x0169,
    PS2_SC_PAGE_UP         = 0x017D,
    PS2_SC_PAGE_DOWN       = 0x017A,

    PS2_SC_UP              = 0x0175,
    PS2_SC_DOWN            = 0x0172,
    PS2_SC_LEFT            = 0x016B,
    PS2_SC_RIGHT           = 0x0174,

    PS2_SC_LEFT_GUI        = 0x11F,
    PS2_SC_RIGHT_GUI       = 0x127,

    /* Print screen / special */

    // PS2_SC_PRINT_SCREEN  = special sequence:
    // E0 12 E0 7C

    // PS2_SC_PAUSE         = special sequence:
    // E1 14 77 E1 F0 14 F0 77


    /* Multimedia / browser keys */

    PS2_SC_VOLUME_UP       = 0x0132,
    PS2_SC_VOLUME_DOWN     = 0x0121,
    PS2_SC_VOLUME_MUTE     = 0x0123,

    PS2_SC_MEDIA_NEXT      = 0x014D,
    PS2_SC_MEDIA_PREV      = 0x0115,
    PS2_SC_MEDIA_STOP      = 0x013B,
    PS2_SC_MEDIA_PLAY      = 0x0134,

    PS2_SC_BROWSER_HOME    = 0x013A,
    PS2_SC_BROWSER_BACK    = 0x0138,
    PS2_SC_BROWSER_FORWARD = 0x0130,
    PS2_SC_BROWSER_REFRESH = 0x0120,
    PS2_SC_BROWSER_STOP    = 0x0128,
    PS2_SC_BROWSER_SEARCH  = 0x0110,
    PS2_SC_BROWSER_FAVOURITES = 0x0118,

} PS2_ScanCode;

int ps2_init();

int ps2_read_scancode();
int ps2_poll_scancode(int* pressed);

void ps2_enable_interrupts(bool enabled);

int ps2_ask_keymap();

int ps2_load_keymap(const char* text, Keymap* keymap);
