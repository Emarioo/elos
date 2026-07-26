
#include "elos/kernel/eve/ps2.h"
#include "elos/kernel/eve/keys.h"


Keymap sv_keymap = {
    .scancode_to_keycode = {
        [PS2_SC_1] = ELOSKEY_1,
        [PS2_SC_2] = ELOSKEY_2,
        [PS2_SC_3] = ELOSKEY_3,
        [PS2_SC_4] = ELOSKEY_4,
        [PS2_SC_5] = ELOSKEY_5,
        [PS2_SC_6] = ELOSKEY_6,
        [PS2_SC_7] = ELOSKEY_7,
        [PS2_SC_8] = ELOSKEY_8,
        [PS2_SC_9] = ELOSKEY_9,
        [PS2_SC_0] = ELOSKEY_0,

        [PS2_SC_Q] = ELOSKEY_Q,
        [PS2_SC_W] = ELOSKEY_W,
        [PS2_SC_E] = ELOSKEY_E,
        [PS2_SC_R] = ELOSKEY_R,
        [PS2_SC_T] = ELOSKEY_T,
        [PS2_SC_Y] = ELOSKEY_Y,
        [PS2_SC_U] = ELOSKEY_U,
        [PS2_SC_I] = ELOSKEY_I,
        [PS2_SC_O] = ELOSKEY_O,
        [PS2_SC_P] = ELOSKEY_P,
        [PS2_SC_A] = ELOSKEY_A,
        [PS2_SC_S] = ELOSKEY_S,
        [PS2_SC_D] = ELOSKEY_D,
        [PS2_SC_F] = ELOSKEY_F,
        [PS2_SC_G] = ELOSKEY_G,
        [PS2_SC_H] = ELOSKEY_H,
        [PS2_SC_J] = ELOSKEY_J,
        [PS2_SC_K] = ELOSKEY_K,
        [PS2_SC_L] = ELOSKEY_L,
        [PS2_SC_Z] = ELOSKEY_Z,
        [PS2_SC_X] = ELOSKEY_X,
        [PS2_SC_C] = ELOSKEY_C,
        [PS2_SC_V] = ELOSKEY_V,
        [PS2_SC_B] = ELOSKEY_B,
        [PS2_SC_N] = ELOSKEY_N,
        [PS2_SC_M] = ELOSKEY_M,


        [PS2_SC_GRAVE] = ELOSKEY_GRAVE,
        [PS2_SC_MINUS] = ELOSKEY_MINUS,
        [PS2_SC_EQUAL] = ELOSKEY_EQUAL,

        [PS2_SC_LEFT_BRACKET] = ELOSKEY_LEFT_BRACKET,
        [PS2_SC_RIGHT_BRACKET] = ELOSKEY_RIGHT_BRACKET,
        [PS2_SC_BACKSLASH] = ELOSKEY_BACKSLASH,

        [PS2_SC_SEMICOLON] = ELOSKEY_SEMICOLON,
        [PS2_SC_APOSTROPHE] = ELOSKEY_APOSTROPHE,

        [PS2_SC_LEFT_GUI] = ELOSKEY_LEFT_SUPER,
        [PS2_SC_RIGHT_GUI] = ELOSKEY_RIGHT_SUPER,

        [PS2_SC_COMMA] = ELOSKEY_COMMA,
        [PS2_SC_PERIOD] = ELOSKEY_PERIOD,
        [PS2_SC_SLASH] = ELOSKEY_SLASH,

        [PS2_SC_ESCAPE] = ELOSKEY_ESCAPE,
        [PS2_SC_TAB] = ELOSKEY_TAB,
        [PS2_SC_CAPS_LOCK] = ELOSKEY_CAPS_LOCK,
        [PS2_SC_LEFT_SHIFT] = ELOSKEY_LEFT_SHIFT,
        [PS2_SC_RIGHT_SHIFT] = ELOSKEY_RIGHT_SHIFT,
        [PS2_SC_LEFT_CTRL] = ELOSKEY_LEFT_CTRL,
        [PS2_SC_LEFT_ALT] = ELOSKEY_LEFT_ALT,
        [PS2_SC_SPACE] = ELOSKEY_SPACE,
        [PS2_SC_ENTER] = ELOSKEY_ENTER,
        [PS2_SC_BACKSPACE] = ELOSKEY_BACKSPACE,
        [PS2_SC_NUM_LOCK] = ELOSKEY_NUM_LOCK,
        [PS2_SC_KP_DIVIDE] = ELOSKEY_NUMPAD_SLASH,
        [PS2_SC_KP_MULTIPLY] = ELOSKEY_NUMPAD_ASTERISK,
        [PS2_SC_KP_MINUS] = ELOSKEY_NUMPAD_MINUS,
        [PS2_SC_KP_PLUS] = ELOSKEY_NUMPAD_PLUS,
        [PS2_SC_KP_7] = ELOSKEY_NUMPAD_7,
        [PS2_SC_KP_8] = ELOSKEY_NUMPAD_8,
        [PS2_SC_KP_9] = ELOSKEY_NUMPAD_9,
        [PS2_SC_KP_4] = ELOSKEY_NUMPAD_4,
        [PS2_SC_KP_5] = ELOSKEY_NUMPAD_5,
        [PS2_SC_KP_6] = ELOSKEY_NUMPAD_6,
        [PS2_SC_KP_1] = ELOSKEY_NUMPAD_1,
        [PS2_SC_KP_2] = ELOSKEY_NUMPAD_2,
        [PS2_SC_KP_3] = ELOSKEY_NUMPAD_3,
        [PS2_SC_KP_0] = ELOSKEY_NUMPAD_0,
        [PS2_SC_KP_PERIOD] = ELOSKEY_NUMPAD_COMMA,
        [PS2_SC_KP_ENTER] = ELOSKEY_NUMPAD_ENTER,

        [PS2_SC_F1] = ELOSKEY_F1,
        [PS2_SC_F2] = ELOSKEY_F2,
        [PS2_SC_F3] = ELOSKEY_F3,
        [PS2_SC_F4] = ELOSKEY_F4,
        [PS2_SC_F5] = ELOSKEY_F5,
        [PS2_SC_F6] = ELOSKEY_F6,
        [PS2_SC_F7] = ELOSKEY_F7,
        [PS2_SC_F8] = ELOSKEY_F8,
        [PS2_SC_F9] = ELOSKEY_F9,
        [PS2_SC_F10] = ELOSKEY_F10,
        [PS2_SC_F11] = ELOSKEY_F11,
        [PS2_SC_F12] = ELOSKEY_F12,

        [PS2_SC_INSERT] = ELOSKEY_INSERT,
        [PS2_SC_DELETE] = ELOSKEY_DELETE,
        [PS2_SC_HOME] = ELOSKEY_HOME,
        [PS2_SC_END] = ELOSKEY_END,
        [PS2_SC_PAGE_UP] = ELOSKEY_PAGE_UP,
        [PS2_SC_PAGE_DOWN] = ELOSKEY_PAGE_DOWN,
        [PS2_SC_UP] = ELOSKEY_UP_ARROW,
        [PS2_SC_DOWN] = ELOSKEY_DOWN_ARROW,
        [PS2_SC_LEFT] = ELOSKEY_LEFT_ARROW,
        [PS2_SC_RIGHT] = ELOSKEY_RIGHT_ARROW,

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

