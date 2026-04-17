
#include "elos/keyboard.h"

#include "elos/kernel/kbd/ps2.h"
#include "elos/kernel/kbd/keys.h"



extern const char* sv_keymap;

void KBD_init(BootAPI* boot_api) {
    ps2_init();
  
    ps2_load_keymap(sv_keymap, &_default_keymap);
}


Keycode KBD_read_key(int* character, int* mods) {
    // BLOCKING
    int scancode = ps2_read_scancode();

    *character = scancode_to_char(scancode, 0);
    return scancode_to_keycode(scancode);
}

                    // keycode scancode
const char* sv_keymap = "1 118\n"
                        "2 18\n"
                        "3 89\n"
                        "4 20\n"
                        "5 57364\n"
                        "6 17\n"
                        "7 57361\n"
                        "8 102\n"
                        "9 13\n"
                        "10 90\n"
                        "12 57452\n"
                        "13 57456\n"
                        "14 57457\n"
                        "15 57449\n"
                        "16 57466\n"
                        "17 57469\n"
                        // "19 20\n" // Super key, can't be detected in qemu
                        "20 88\n"
                        "21 41\n"
                        "28 93\n"
                        "32 78\n"
                        "33 65\n"
                        "34 74\n"
                        "35 73\n"
                        "37 69\n"
                        "38 22\n"
                        "39 30\n"
                        "40 38\n"
                        "41 37\n"
                        "42 46\n"
                        "43 54\n"
                        "44 61\n"
                        "45 62\n"
                        "46 70\n"
                        "54 28\n"
                        "55 50\n"
                        "56 33\n"
                        "57 35\n"
                        "58 36\n"
                        "59 43\n"
                        "60 52\n"
                        "61 51\n"
                        "62 67\n"
                        "63 59\n"
                        "64 66\n"
                        "65 75\n"
                        "66 58\n"
                        "67 49\n"
                        "68 68\n"
                        "69 77\n"
                        "70 21\n"
                        "71 45\n"
                        "72 27\n"
                        "73 44\n"
                        "74 60\n"
                        "75 42\n"
                        "76 29\n"
                        "77 34\n"
                        "78 53\n"
                        "79 26\n"
                        "85 85\n"
                        "90 57451\n"
                        "91 57460\n"
                        "92 57461\n"
                        "93 57458\n"
                        "94 5\n"
                        "95 6\n"
                        "96 4\n"
                        "97 12\n"
                        "98 3\n"
                        "99 11\n"
                        "100 131\n"
                        "101 10\n"
                        "102 1\n"
                        "103 9\n"
                        "104 120\n"
                        "105 7\n";
