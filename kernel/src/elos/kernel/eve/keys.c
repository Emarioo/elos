
#include "elos/common/types.h"

#include "elos/kernel/eve/keys.h"

const char* key_name(ELOS_Keycode keycode) {
    switch (keycode) {
        case KEY_ESCAPE:            return "Escape";
        case KEY_LEFT_SHIFT:        return "LeftShift";
        case KEY_RIGHT_SHIFT:       return "RightShift";
        case KEY_LEFT_CTRL:         return "LeftCtrl";
        case KEY_RIGHT_CTRL:        return "RightCtrl";
        case KEY_LEFT_ALT:          return "LeftAlt";
        case KEY_RIGHT_ALT:         return "RightAlt";
        case KEY_BACKSPACE:         return "Backspace";
        case KEY_TAB:               return "Tab";
        case KEY_ENTER:             return "Enter";
        case KEY_HOME:              return "Home";
        case KEY_INSERT:            return "Insert";
        case KEY_DELETE:            return "Delete";
        case KEY_END:               return "End";
        case KEY_PAGE_DOWN:         return "PageUp";
        case KEY_PAGE_UP:           return "PageDown";
        case KEY_NUM_LOCK:          return "NumLock";
        case KEY_SUPER:             return "Super";
        case KEY_CAPS_LOCK:          return "CapsLock";
        case KEY_SPACE:             return "Space";
        case KEY_LEFT_PAREN:        return "(";
        case KEY_RIGHT_PAREN:       return ")";
        case KEY_PLUS:              return "+";
        case KEY_COMMA:             return ",";
        case KEY_MINUS:             return "-";
        case KEY_GRAVE:             return "Grave";
        case KEY_PERIOD:            return ".";
        case KEY_SLASH:             return "/";
        case KEY_0:                 return "0";
        case KEY_1:                 return "1";
        case KEY_2:                 return "2";
        case KEY_3:                 return "3";
        case KEY_4:                 return "4";
        case KEY_5:                 return "5";
        case KEY_6:                 return "6";
        case KEY_7:                 return "7";
        case KEY_8:                 return "8";
        case KEY_9:                 return "9";
        case KEY_APOSTROPHE:        return "Apostrophe";
        case KEY_ARROW:             return "Arrow";
        case KEY_COLON:             return ":";
        case KEY_SEMICOLON:         return ";";
        case KEY_EQUAL:             return "=";
        case KEY_A:                 return "A";
        case KEY_B:                 return "B";
        case KEY_C:                 return "C";
        case KEY_D:                 return "D";
        case KEY_E:                 return "E";
        case KEY_F:                 return "F";
        case KEY_G:                 return "G";
        case KEY_H:                 return "H";
        case KEY_I:                 return "I";
        case KEY_J:                 return "J";
        case KEY_K:                 return "K";
        case KEY_L:                 return "L";
        case KEY_M:                 return "M";
        case KEY_N:                 return "N";
        case KEY_O:                 return "O";
        case KEY_P:                 return "P";
        case KEY_Q:                 return "Q";
        case KEY_R:                 return "R";
        case KEY_S:                 return "S";
        case KEY_T:                 return "T";
        case KEY_U:                 return "U";
        case KEY_V:                 return "V";
        case KEY_W:                 return "W";
        case KEY_X:                 return "X";
        case KEY_Y:                 return "Y";
        case KEY_Z:                 return "Z";
        case KEY_LEFT_BRACKET:      return "[";
        case KEY_BACKSLASH:         return "\\";
        case KEY_RIGHT_BRACKET:     return "]";
        case KEY_LEFT_ARROW:        return "LeftArrow";
        case KEY_RIGHT_ARROW:       return "RightArrow";
        case KEY_UP_ARROW:          return "UpArrow";
        case KEY_DOWN_ARROW:        return "DownArrow";
        case KEY_F1:                return "F1";
        case KEY_F2:                return "F2";
        case KEY_F3:                return "F3";
        case KEY_F4:                return "F4";
        case KEY_F5:                return "F5";
        case KEY_F6:                return "F6";
        case KEY_F7:                return "F7";
        case KEY_F8:                return "F8";
        case KEY_F9:                return "F9";
        case KEY_F10:               return "F10";
        case KEY_F11:               return "F11";
        case KEY_F12:               return "F12";
        case KEY_F13:               return "F13";
        case KEY_F14:               return "F14";
        case KEY_F15:               return "F15";
        case KEY_NUMPAD_0:          return "NumPad0";
        case KEY_NUMPAD_1:          return "NumPad1";
        case KEY_NUMPAD_2:          return "NumPad2";
        case KEY_NUMPAD_3:          return "NumPad3";
        case KEY_NUMPAD_4:          return "NumPad4";
        case KEY_NUMPAD_5:          return "NumPad5";
        case KEY_NUMPAD_6:          return "NumPad6";
        case KEY_NUMPAD_7:          return "NumPad7";
        case KEY_NUMPAD_8:          return "NumPad8";
        case KEY_NUMPAD_9:          return "NumPad9";
        case KEY_NUMPAD_COMMA:      return "NumPadComma";
        case KEY_NUMPAD_SLASH:      return "NumPadSlash";
        case KEY_NUMPAD_ASTERISK:   return "NumPadAsterisk";
        case KEY_NUMPAD_MINUS:      return "NumPadMinux";
        case KEY_NUMPAD_PLUS:       return "NumPadPlus";
        case KEY_NUMPAD_ENTER:      return "NumPadEnter";
    };
    return NULL;
}


ELOS_Keycode scancode_to_keycode(Keymap* keymap, u32 scancode) {
    return keymap->scancode_to_keycode[scancode];
}


u32 scancode_to_character(Keymap* keymap, u32 scancode, u32 mod, ELOS_Keycode* out_keycode) {
    ELOS_Keycode keycode = scancode_to_keycode(keymap, scancode);
    if (out_keycode) {
        *out_keycode = keycode;
    }

    int isLetter = keycode >= KEY_A && keycode <= KEY_Z;
    for (int i=0;i<ARRAY_LENGTH(keymap->extra_capslock_scancodes);i++) {
        if (keymap->extra_capslock_scancodes[i] == scancode) {
            isLetter = true;
            break;
        }
    }

    int isNumpad = keycode >= KEY_NUMPAD_0 && keycode <= KEY_NUMPAD_ENTER;

    int index;
    if (isLetter) {
        index = ((!(mod & KEY_MOD_SHIFT) ^ !(mod & KEY_MOD_CAPS_LOCK)) ? 1 : 0) + ( (mod & KEY_MOD_ALT) ? 2 : 0);
    } else if (isNumpad) {
        index = ((!(mod & KEY_MOD_SHIFT) ^ !(mod & KEY_MOD_NUM_LOCK)) ? 0 : 1) + ( (mod & KEY_MOD_ALT) ? 2 : 0);
    } else {
        index = ((mod & KEY_MOD_SHIFT) ? 1 : 0) + ( (mod & KEY_MOD_ALT) ? 2 : 0);
    }

    return keymap->scancode_to_char[scancode][index];
}

