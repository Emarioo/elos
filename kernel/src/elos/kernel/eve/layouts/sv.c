
#include "elos/kernel/eve/ps2.h"
#include "elos/kernel/eve/keys.h"


Keymap sv_keymap = {
    .scancode_to_keycode = {
        [PS2_SC_1] = KEY_1,
        [PS2_SC_2] = KEY_2,
        [PS2_SC_3] = KEY_3,
        [PS2_SC_4] = KEY_4,
        [PS2_SC_5] = KEY_5,
        [PS2_SC_6] = KEY_6,
        [PS2_SC_7] = KEY_7,
        [PS2_SC_8] = KEY_8,
        [PS2_SC_9] = KEY_9,
        [PS2_SC_0] = KEY_0,

        [PS2_SC_Q] = KEY_Q,
        [PS2_SC_W] = KEY_W,
        [PS2_SC_E] = KEY_E,
        [PS2_SC_R] = KEY_R,
        [PS2_SC_T] = KEY_T,
        [PS2_SC_Y] = KEY_Y,
        [PS2_SC_U] = KEY_U,
        [PS2_SC_I] = KEY_I,
        [PS2_SC_O] = KEY_O,
        [PS2_SC_P] = KEY_P,
        [PS2_SC_A] = KEY_A,
        [PS2_SC_S] = KEY_S,
        [PS2_SC_D] = KEY_D,
        [PS2_SC_F] = KEY_F,
        [PS2_SC_G] = KEY_G,
        [PS2_SC_H] = KEY_H,
        [PS2_SC_J] = KEY_J,
        [PS2_SC_K] = KEY_K,
        [PS2_SC_L] = KEY_L,
        [PS2_SC_Z] = KEY_Z,
        [PS2_SC_X] = KEY_X,
        [PS2_SC_C] = KEY_C,
        [PS2_SC_V] = KEY_V,
        [PS2_SC_B] = KEY_B,
        [PS2_SC_N] = KEY_N,
        [PS2_SC_M] = KEY_M,


        [PS2_SC_GRAVE] = KEY_GRAVE,
        [PS2_SC_MINUS] = KEY_MINUS,
        [PS2_SC_EQUAL] = KEY_EQUAL,

        [PS2_SC_LEFT_BRACKET] = KEY_LEFT_BRACKET,
        [PS2_SC_RIGHT_BRACKET] = KEY_RIGHT_BRACKET,
        [PS2_SC_BACKSLASH] = KEY_BACKSLASH,

        [PS2_SC_SEMICOLON] = KEY_SEMICOLON,
        [PS2_SC_APOSTROPHE] = KEY_APOSTROPHE,

        [PS2_SC_LEFT_GUI] = KEY_LEFT_SUPER,
        [PS2_SC_RIGHT_GUI] = KEY_RIGHT_SUPER,

        [PS2_SC_COMMA] = KEY_COMMA,
        [PS2_SC_PERIOD] = KEY_PERIOD,
        [PS2_SC_SLASH] = KEY_SLASH,

        [PS2_SC_ESCAPE] = KEY_ESCAPE,
        [PS2_SC_TAB] = KEY_TAB,
        [PS2_SC_CAPS_LOCK] = KEY_CAPS_LOCK,
        [PS2_SC_LEFT_SHIFT] = KEY_LEFT_SHIFT,
        [PS2_SC_RIGHT_SHIFT] = KEY_RIGHT_SHIFT,
        [PS2_SC_LEFT_CTRL] = KEY_LEFT_CTRL,
        [PS2_SC_LEFT_ALT] = KEY_LEFT_ALT,
        [PS2_SC_SPACE] = KEY_SPACE,
        [PS2_SC_ENTER] = KEY_ENTER,
        [PS2_SC_BACKSPACE] = KEY_BACKSPACE,
        [PS2_SC_NUM_LOCK] = KEY_NUM_LOCK,
        [PS2_SC_KP_DIVIDE] = KEY_NUMPAD_SLASH,
        [PS2_SC_KP_MULTIPLY] = KEY_NUMPAD_ASTERISK,
        [PS2_SC_KP_MINUS] = KEY_NUMPAD_MINUS,
        [PS2_SC_KP_PLUS] = KEY_NUMPAD_PLUS,
        [PS2_SC_KP_7] = KEY_NUMPAD_7,
        [PS2_SC_KP_8] = KEY_NUMPAD_8,
        [PS2_SC_KP_9] = KEY_NUMPAD_9,
        [PS2_SC_KP_4] = KEY_NUMPAD_4,
        [PS2_SC_KP_5] = KEY_NUMPAD_5,
        [PS2_SC_KP_6] = KEY_NUMPAD_6,
        [PS2_SC_KP_1] = KEY_NUMPAD_1,
        [PS2_SC_KP_2] = KEY_NUMPAD_2,
        [PS2_SC_KP_3] = KEY_NUMPAD_3,
        [PS2_SC_KP_0] = KEY_NUMPAD_0,
        [PS2_SC_KP_PERIOD] = KEY_NUMPAD_COMMA,
        [PS2_SC_KP_ENTER] = KEY_NUMPAD_ENTER,

        [PS2_SC_F1] = KEY_F1,
        [PS2_SC_F2] = KEY_F2,
        [PS2_SC_F3] = KEY_F3,
        [PS2_SC_F4] = KEY_F4,
        [PS2_SC_F5] = KEY_F5,
        [PS2_SC_F6] = KEY_F6,
        [PS2_SC_F7] = KEY_F7,
        [PS2_SC_F8] = KEY_F8,
        [PS2_SC_F9] = KEY_F9,
        [PS2_SC_F10] = KEY_F10,
        [PS2_SC_F11] = KEY_F11,
        [PS2_SC_F12] = KEY_F12,

        [PS2_SC_INSERT] = KEY_INSERT,
        [PS2_SC_DELETE] = KEY_DELETE,
        [PS2_SC_HOME] = KEY_HOME,
        [PS2_SC_END] = KEY_END,
        [PS2_SC_PAGE_UP] = KEY_PAGE_UP,
        [PS2_SC_PAGE_DOWN] = KEY_PAGE_DOWN,
        [PS2_SC_UP] = KEY_UP_ARROW,
        [PS2_SC_DOWN] = KEY_DOWN_ARROW,
        [PS2_SC_LEFT] = KEY_LEFT_ARROW,
        [PS2_SC_RIGHT] = KEY_RIGHT_ARROW,

        // PS2_SC_VOLUME_UP
        // PS2_SC_VOLUME_DOWN
        // PS2_SC_VOLUME_MUTE
        // PS2_SC_MEDIA_NEXT
        // PS2_SC_MEDIA_PREV
        // PS2_SC_MEDIA_STOP
        // PS2_SC_MEDIA_PLAY
        // PS2_SC_BROWSER_HOME
        // PS2_SC_BROWSER_BACK
        // PS2_SC_BROWSER_FORWARD
        // PS2_SC_BROWSER_REFRESH
        // PS2_SC_BROWSER_STOP
        // PS2_SC_BROWSER_SEARCH
        // PS2_SC_BROWSER_FAVOURITES
        // PS2_SC_PRINT_SCREEN

    },

    .scancode_to_char = {
        //     normal shift altgr shift+altgr
        [PS2_SC_1] = { '1', '!', '¡', '¹' },
        [PS2_SC_2] = { '2', '"', '@', '²' },
        [PS2_SC_3] = { '3', '#', '£', '³' },
        [PS2_SC_4] = { '4', '¤', '$', '¼' },
        [PS2_SC_5] = { '5', '%', '€', 0   },
        [PS2_SC_6] = { '6', '&', '¥', '√' },
        [PS2_SC_7] = { '7', '/', '{', '÷' },
        [PS2_SC_8] = { '8', '(', '[', '«' },
        [PS2_SC_9] = { '9', ')', ']', '»' },
        [PS2_SC_0] = { '0', '=', '}', '°' },

        [PS2_SC_Q] = { 'q', 'Q', 'ω', 'Ω' },
        [PS2_SC_W] = { 'w', 'W', 'σ', 'Σ' },
        [PS2_SC_E] = { 'e', 'E', '€', '¢' },
        [PS2_SC_R] = { 'r', 'R', '®', '™' },
        [PS2_SC_T] = { 't', 'T', 'þ', 'Þ' },
        [PS2_SC_Y] = { 'y', 'Y', '←', '¥' },
        [PS2_SC_U] = { 'u', 'U', 0, 0 },
        [PS2_SC_I] = { 'i', 'I', 0, 0 },
        [PS2_SC_O] = { 'o', 'O', 0, 0 },
        [PS2_SC_P] = { 'p', 'P', 0, 0 },
        [PS2_SC_A] = { 'a', 'A', 0, 0 },
        [PS2_SC_S] = { 's', 'S', 0, 0 },
        [PS2_SC_D] = { 'd', 'D', 0, 0 },
        [PS2_SC_F] = { 'f', 'F', 0, 0 },
        [PS2_SC_G] = { 'g', 'G', 0, 0 },
        [PS2_SC_H] = { 'h', 'H', 0, 0 },
        [PS2_SC_J] = { 'j', 'J', 0, 0 },
        [PS2_SC_K] = { 'k', 'K', 0, 0 },
        [PS2_SC_L] = { 'l', 'L', 0, 0 },
        [PS2_SC_Z] = { 'z', 'Z', 0, 0 },
        [PS2_SC_X] = { 'x', 'X', 0, 0 },
        [PS2_SC_C] = { 'c', 'C', 0, 0 },
        [PS2_SC_V] = { 'v', 'V', 0, 0 },
        [PS2_SC_B] = { 'b', 'B', 0, 0 },
        [PS2_SC_N] = { 'n', 'N', 0, 0 },
        [PS2_SC_M] = { 'm', 'M', 0, 0 },

        [PS2_SC_GRAVE] = { 'ö', 'Ö', 'æ', 'Æ' },
        [PS2_SC_MINUS] = { '+', '?', '\\', '¿' },
        
        [PS2_SC_LEFT_BRACKET] = { 'å', 'Å', '¨', 0 },
        [PS2_SC_BACKSLASH] = { '\'', '*', '´', '×' },
        
        [PS2_SC_APOSTROPHE] = { 'ä', 'Ä', 'æ', 'Æ' },
        
        [PS2_SC_COMMA] = { ',', ';', '¸', '˛' },
        [PS2_SC_PERIOD] = { '.', ':', '·', '…' },
        [PS2_SC_ARROW] = { '<', '>', '|', '¦' },
        [PS2_SC_SLASH] = { '-', '_', 0, 0 },

        [PS2_SC_SPACE] = { ' ', ' ', ' ', ' '},
        [PS2_SC_ENTER] = { '\n', '\n', '\n', '\n' },

        [PS2_SC_KP_DIVIDE]   = { '/', '/', '/', '/' },
        [PS2_SC_KP_MULTIPLY] = { '*', '*', '*', '*' },
        [PS2_SC_KP_MINUS]    = { '-', '-', '-', '-' },
        [PS2_SC_KP_PLUS]     = { '+', '+', '+', '+' },
        [PS2_SC_KP_7]        = { '7', 0, '7', 0 },
        [PS2_SC_KP_8]        = { '8', 0, '8', 0 },
        [PS2_SC_KP_9]        = { '9', 0, '9', 0 },
        [PS2_SC_KP_4]        = { '4', 0, '4', 0 },
        [PS2_SC_KP_5]        = { '5', 0, '5', 0 },
        [PS2_SC_KP_6]        = { '6', 0, '6', 0 },
        [PS2_SC_KP_1]        = { '1', 0, '1', 0 },
        [PS2_SC_KP_2]        = { '2', 0, '2', 0 },
        [PS2_SC_KP_3]        = { '3', 0, '3', 0 },
        [PS2_SC_KP_0]        = { '0', 0, '0', 0 },
        [PS2_SC_KP_PERIOD]   = { ',', 0, ',', 0 },
        [PS2_SC_KP_ENTER]    = { '\n', '\n', '\n', '\n' }
    },
    .extra_capslock_scancodes = {
        PS2_SC_LEFT_BRACKET, PS2_SC_APOSTROPHE, PS2_SC_GRAVE,
    },
};

