/*
    Main terminal
*/

#include "elos/keyboard.h"
#include "elos/kernel/video/frame.h"
#include "elos/frame_buffer.h"
#include "elos/common/types.h"
#include "elos/common/string.h"
#include "elos/common/intrinsics.h"

#include "elos/cpu.h"
#include "elos/execution.h"
#include "elos/kernel_console.h"

#include "user/pipe.h"

int ring_size = 0x10000;
Handle ringBuffer;

#define printf(...) KCON_printf(__VA_ARGS__)

void proc1();
void proc2();

void terminal_main() {

    printf("Starting terminal\n");
    
    int res = create_ringBuffer(ring_size, &ringBuffer);
    if (res < 0) {
        printf("Could not create ring buffer\n");
        return;
    }

    int coreCount = CPU_get_core_count();
    for (int i=0;i<coreCount;i++) {
        EXEC_create_thread(proc1, i);
        EXEC_create_thread(proc2, i);
    }

    while (1) pause();
}


#define MS 1000000


typedef struct {
    char text[16];
} Message;

void proc1() {
    int core = CPU_get_core_index();
    printf("Proc 1, on core: %d\n", core);

    // while (1) pause();

    int index = 0;

    Message messages[] = {
        { "Hello" },
        { "World" },
        { "Writing" },
        { "Bytes" },
        { "To you." },
    };

    while (1) {
        int written = write(ringBuffer, &messages[index], sizeof(messages[index]));
        index = (index + 1) % ARRAY_LENGTH(messages);

        printf("C%d Sent %d\n", core, index);
        CPU_sleep(10 * MS);
    }
}

void proc2() {
    int core = CPU_get_core_index();
    printf("Proc 2, on core: %d\n", core);

    // while (1) pause();

    Message msg;

    while (1) {
        int read_bytes = read(ringBuffer, &msg, sizeof(msg));
        printf("C%d Read: %s\n", core, msg.text);

        CPU_sleep(300 * MS);
    }
}


// char _text_buffer[4096];

// string g_terminal_text;
// int g_terminal_cursor_pos;
// int g_terminal_text_x = 10;
// int g_terminal_text_y = 10;


// void apply_command(cstring text);

// void edit_text(string* text, Keycode keycode, int character, int mods, int* cursor) {

//     if (keycode == KEY_LEFT_ARROW) {
//         if (*cursor > 0) {
//             (*cursor)--;
//         }
//     } else if (keycode == KEY_RIGHT_ARROW) {
//         if (*cursor < text->len) {
//             (*cursor)++;
//         }
//     } else if (keycode == KEY_DELETE) {
//         if (text->len <= *cursor ) {
//             return;
//         }
//         memmove(text->ptr + *cursor, text->ptr + *cursor+1, text->len - *cursor - 1);
//         text->len--;
//     } else if (keycode == KEY_BACKSPACE) {
//         if (*cursor <= 0 ) {
//             return;
//         } else {
//             memmove(text->ptr + *cursor - 1, text->ptr + *cursor, text->len - *cursor);
//             (*cursor)--;
//             text->len--;
//         }
//     } else {
//         if (character == 0)
//             return; // not a char code.
//         if (*cursor == text->len) {
//             text->ptr[*cursor] = character;
//             text->len++;
//             (*cursor)++;
//             text->ptr[*cursor] = '\0';
//         } else {
//             memmove(text->ptr + *cursor + 1, text->ptr + *cursor, text->len - *cursor);
//             text->ptr[*cursor] = character;
//             text->len++;
//             (*cursor)++;
//         }
//     }
// }

// void terminal_start() {
//     g_terminal_text.ptr = _text_buffer;
//     g_terminal_text.max = sizeof(_text_buffer);
//     g_terminal_text.len = 0;

//     char buffer[50];

//     int text_height = 20;
//     while (1) {


//         int character;
//         int mods;
//         Keycode keycode = KBD_read_key(&character, &mods);

//         if (keycode == KEY_ENTER) {
//             g_terminal_cursor_pos = 0;
//             g_terminal_text.len = 0;
//             g_terminal_text_y += text_height;

//             apply_command(STR_CSTR(g_terminal_text));
//         } else {
//             edit_text(&g_terminal_text, keycode, character, mods, &g_terminal_cursor_pos);
//         }

//         //  debug scancode
//         snprintf(buffer, sizeof(buffer), "scancode %d", keycode);
//         draw_glyphs_from_text_bcolor(500, 500, text_height, PTR_CSTR(buffer), g_default_font, WHITE, DARK_BLUE);

//         int text_width = draw_text_width(STR_CSTR(g_terminal_text), text_height, g_default_font);
//         draw_glyphs_from_text_bcolor(g_terminal_text_x, g_terminal_text_y, text_height, STR_CSTR(g_terminal_text), g_default_font, WHITE, DARK_BLUE);
//         draw_rect(g_terminal_text_x + text_width, g_terminal_text_y, 40, text_height, DARK_BLUE);

//         // Cursor
//         cstring temp;
//         temp.ptr = g_terminal_text.ptr;
//         temp.len = g_terminal_cursor_pos;
//         int width = draw_text_width(temp, text_height, g_default_font);
//         draw_rect(g_terminal_text_x + width, g_terminal_text_y, 2, text_height, WHITE);
//     }

// }


// void apply_command(cstring text) {

//     // Parse command

//     // root := "/home/user"

//     // list_files(root)

// }

