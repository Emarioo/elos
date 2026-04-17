#pragma once

#include "elos/boot_api.h"

typedef enum Keycode Keycode;


void KBD_init(BootAPI* boot_api);

// @param character is Unicode codepoint which is why it isn't char type (but we just support ASCII at the moment)
Keycode KBD_read_key(int* character, int* mods);
// int KBD_read_char();


enum Keycode {
    KEY_NONE, // empty/invalid key
    KEY_ESCAPE,
    KEY_LSHIFT,
    KEY_RSHIFT,
    KEY_LCTRL,
    KEY_RCTRL,
    KEY_LALT,
    KEY_RALT,
    KEY_BACKSPACE,
    KEY_TAB,
    KEY_ENTER,
    KEY_FN,
    KEY_HOME,
    KEY_INSERT,
    KEY_DELETE,
    KEY_END,
    KEY_PAGE_DOWN,
    KEY_PAGE_UP,
    KEY_NUM_LOCK,
    KEY_SUPER,
    KEY_CAPSLOCK,

    KEY_SPACE,
    KEY_EXCLAMATION_MARK,
    KEY_DQUOTE,
    KEY_HASHTAG,
    KEY_DOLLAR,
    KEY_PERCENT,
    KEY_AMPERSAND,
    KEY_SQUOTE,
    KEY_LPAREN,
    KEY_RPAREN,
    KEY_ASTERISK,
    KEY_PLUS,
    KEY_COMMA,
    KEY_MINUS,
    KEY_PERIOD,
    KEY_SLASH,

    KEY_0,
    KEY_1,
    KEY_2,
    KEY_3,
    KEY_4,
    KEY_5,
    KEY_6,
    KEY_7,
    KEY_8,
    KEY_9,

    KEY_COLON,
    KEY_SEMICOLON,
    KEY_LESSER,
    KEY_EQUAL,
    KEY_GREATER,
    KEY_QUESTION_MARK,
    KEY_AT_SIGN,


    KEY_A,
    KEY_B,
    KEY_C,
    KEY_D,
    KEY_E,
    KEY_F,
    KEY_G,
    KEY_H,
    KEY_I,
    KEY_J,
    KEY_K,
    KEY_L,
    KEY_M,
    KEY_N,
    KEY_O,
    KEY_P,
    KEY_Q,
    KEY_R,
    KEY_S,
    KEY_T,
    KEY_U,
    KEY_V,
    KEY_W,
    KEY_X,
    KEY_Y,
    KEY_Z,

    KEY_LBRACKET,
    KEY_BACKSLASH,
    KEY_RBRACKET,
    KEY_CARET,
    KEY_UNDERSCORE,
    KEY_BACKTICK,

    KEY_LBRACE,
    KEY_VERTICAL_BAR,
    KEY_RBRACE,
    KEY_TILDE,

    KEY_LEFT_ARROW,
    KEY_RIGHT_ARROW,
    KEY_UP_ARROW,
    KEY_DOWN_ARROW,

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
    KEY_F13,
    KEY_F14,
    KEY_F15,

    KEY_MAX,

    KEY_MOD_SHIFT    = 0x1,
    KEY_MOD_ALT      = 0x2,
    KEY_MOD_CAPSLOCK = 0x4,
    KEY_MOD_CTRL     = 0x8,
};
