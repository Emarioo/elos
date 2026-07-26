
#include "elos/common/types.h"

#include "elos/kernel/eve/keys.h"

const char* key_name(ELOS_Keycode keycode) {
    switch (keycode) {
        case ELOSKEY_ESCAPE:            return "Escape";
        case ELOSKEY_LEFT_SHIFT:        return "LeftShift";
        case ELOSKEY_RIGHT_SHIFT:       return "RightShift";
        case ELOSKEY_LEFT_CTRL:         return "LeftCtrl";
        case ELOSKEY_RIGHT_CTRL:        return "RightCtrl";
        case ELOSKEY_LEFT_ALT:          return "LeftAlt";
        case ELOSKEY_RIGHT_ALT:         return "RightAlt";
        case ELOSKEY_BACKSPACE:         return "Backspace";
        case ELOSKEY_TAB:               return "Tab";
        case ELOSKEY_ENTER:             return "Enter";
        case ELOSKEY_HOME:              return "Home";
        case ELOSKEY_INSERT:            return "Insert";
        case ELOSKEY_DELETE:            return "Delete";
        case ELOSKEY_END:               return "End";
        case ELOSKEY_PAGE_DOWN:         return "PageUp";
        case ELOSKEY_PAGE_UP:           return "PageDown";
        case ELOSKEY_NUM_LOCK:          return "NumLock";
        case ELOSKEY_LEFT_SUPER:        return "LeftSuper";
        case ELOSKEY_RIGHT_SUPER:       return "RightSuper";
        case ELOSKEY_CAPS_LOCK:         return "CapsLock";
        case ELOSKEY_SPACE:             return "Space";
        case ELOSKEY_LEFT_PAREN:        return "(";
        case ELOSKEY_RIGHT_PAREN:       return ")";
        case ELOSKEY_PLUS:              return "+";
        case ELOSKEY_COMMA:             return ",";
        case ELOSKEY_MINUS:             return "-";
        case ELOSKEY_GRAVE:             return "Grave";
        case ELOSKEY_PERIOD:            return ".";
        case ELOSKEY_SLASH:             return "/";
        case ELOSKEY_0:                 return "0";
        case ELOSKEY_1:                 return "1";
        case ELOSKEY_2:                 return "2";
        case ELOSKEY_3:                 return "3";
        case ELOSKEY_4:                 return "4";
        case ELOSKEY_5:                 return "5";
        case ELOSKEY_6:                 return "6";
        case ELOSKEY_7:                 return "7";
        case ELOSKEY_8:                 return "8";
        case ELOSKEY_9:                 return "9";
        case ELOSKEY_APOSTROPHE:        return "Apostrophe";
        case ELOSKEY_ARROW:             return "Arrow";
        case ELOSKEY_COLON:             return ":";
        case ELOSKEY_SEMICOLON:         return ";";
        case ELOSKEY_EQUAL:             return "=";
        case ELOSKEY_A:                 return "A";
        case ELOSKEY_B:                 return "B";
        case ELOSKEY_C:                 return "C";
        case ELOSKEY_D:                 return "D";
        case ELOSKEY_E:                 return "E";
        case ELOSKEY_F:                 return "F";
        case ELOSKEY_G:                 return "G";
        case ELOSKEY_H:                 return "H";
        case ELOSKEY_I:                 return "I";
        case ELOSKEY_J:                 return "J";
        case ELOSKEY_K:                 return "K";
        case ELOSKEY_L:                 return "L";
        case ELOSKEY_M:                 return "M";
        case ELOSKEY_N:                 return "N";
        case ELOSKEY_O:                 return "O";
        case ELOSKEY_P:                 return "P";
        case ELOSKEY_Q:                 return "Q";
        case ELOSKEY_R:                 return "R";
        case ELOSKEY_S:                 return "S";
        case ELOSKEY_T:                 return "T";
        case ELOSKEY_U:                 return "U";
        case ELOSKEY_V:                 return "V";
        case ELOSKEY_W:                 return "W";
        case ELOSKEY_X:                 return "X";
        case ELOSKEY_Y:                 return "Y";
        case ELOSKEY_Z:                 return "Z";
        case ELOSKEY_LEFT_BRACKET:      return "[";
        case ELOSKEY_BACKSLASH:         return "\\";
        case ELOSKEY_RIGHT_BRACKET:     return "]";
        case ELOSKEY_LEFT_ARROW:        return "LeftArrow";
        case ELOSKEY_RIGHT_ARROW:       return "RightArrow";
        case ELOSKEY_UP_ARROW:          return "UpArrow";
        case ELOSKEY_DOWN_ARROW:        return "DownArrow";
        case ELOSKEY_F1:                return "F1";
        case ELOSKEY_F2:                return "F2";
        case ELOSKEY_F3:                return "F3";
        case ELOSKEY_F4:                return "F4";
        case ELOSKEY_F5:                return "F5";
        case ELOSKEY_F6:                return "F6";
        case ELOSKEY_F7:                return "F7";
        case ELOSKEY_F8:                return "F8";
        case ELOSKEY_F9:                return "F9";
        case ELOSKEY_F10:               return "F10";
        case ELOSKEY_F11:               return "F11";
        case ELOSKEY_F12:               return "F12";
        case ELOSKEY_F13:               return "F13";
        case ELOSKEY_F14:               return "F14";
        case ELOSKEY_F15:               return "F15";
        case ELOSKEY_NUMPAD_0:          return "NumPad0";
        case ELOSKEY_NUMPAD_1:          return "NumPad1";
        case ELOSKEY_NUMPAD_2:          return "NumPad2";
        case ELOSKEY_NUMPAD_3:          return "NumPad3";
        case ELOSKEY_NUMPAD_4:          return "NumPad4";
        case ELOSKEY_NUMPAD_5:          return "NumPad5";
        case ELOSKEY_NUMPAD_6:          return "NumPad6";
        case ELOSKEY_NUMPAD_7:          return "NumPad7";
        case ELOSKEY_NUMPAD_8:          return "NumPad8";
        case ELOSKEY_NUMPAD_9:          return "NumPad9";
        case ELOSKEY_NUMPAD_COMMA:      return "NumPadComma";
        case ELOSKEY_NUMPAD_SLASH:      return "NumPadSlash";
        case ELOSKEY_NUMPAD_ASTERISK:   return "NumPadAsterisk";
        case ELOSKEY_NUMPAD_MINUS:      return "NumPadMinux";
        case ELOSKEY_NUMPAD_PLUS:       return "NumPadPlus";
        case ELOSKEY_NUMPAD_ENTER:      return "NumPadEnter";
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

    int isLetter = keycode >= ELOSKEY_A && keycode <= ELOSKEY_Z;
    for (int i=0;i<ARRAY_LENGTH(keymap->extra_capslock_scancodes);i++) {
        if (keymap->extra_capslock_scancodes[i] == scancode) {
            isLetter = true;
            break;
        }
    }

    int isNumpad = keycode >= ELOSKEY_NUMPAD_0 && keycode <= ELOSKEY_NUMPAD_ENTER;

    int index;
    if (isLetter) {
        index = ((!(mod & ELOSKEY_MOD_SHIFT) ^ !(mod & ELOSKEY_MOD_CAPS_LOCK)) ? 1 : 0) + ( (mod & ELOSKEY_MOD_ALT) ? 2 : 0);
    } else if (isNumpad) {
        index = ((!(mod & ELOSKEY_MOD_SHIFT) ^ !(mod & ELOSKEY_MOD_NUM_LOCK)) ? 0 : 1) + ( (mod & ELOSKEY_MOD_ALT) ? 2 : 0);
    } else {
        index = ((mod & ELOSKEY_MOD_SHIFT) ? 1 : 0) + ( (mod & ELOSKEY_MOD_ALT) ? 2 : 0);
    }

    return keymap->scancode_to_char[scancode][index];
}

