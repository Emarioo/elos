/*
    Main terminal
*/

#include "elos/kernel/driver/ps2.h"
#include "elos/kernel/frame/frame.h"
#include "elos/kernel/common/types.h"
#include "elos/kernel/common/string.h"


char _text_buffer[4096];

string g_terminal_text;
int g_terminal_cursor_pos;
int g_terminal_text_x = 10;
int g_terminal_text_y = 10;

void edit_text(string* text, int scancode, int* cursor) {
    int keycode = scancode_to_keycode(scancode);

    if (keycode == KEY_LEFT_ARROW) {
        if (*cursor > 0) {
            (*cursor)--;
        }
    } else if (keycode == KEY_RIGHT_ARROW) {
        if (*cursor < text->len) {
            (*cursor)++;
        }
    } else if (keycode == KEY_DELETE) {
        if (text->len <= *cursor ) {
            return;
        }
        memmove(text->ptr + *cursor, text->ptr + *cursor+1, text->len - *cursor - 1);
        text->len--;
    } else if (keycode == KEY_BACKSPACE) {
        if (*cursor <= 0 ) {
            return;
        } else {
            memmove(text->ptr + *cursor - 1, text->ptr + *cursor, text->len - *cursor);
            (*cursor)--;
            text->len--;
        }
    } else {
        char chr = scancode_to_char(scancode, 0);
        if (chr == 0)
            return; // not a char code.
        if (*cursor == text->len) {
            text->ptr[*cursor] = chr;
            text->len++;
            (*cursor)++;
            text->ptr[*cursor] = '\0';
        } else {
            memmove(text->ptr + *cursor + 1, text->ptr + *cursor, text->len - *cursor);
            text->ptr[*cursor] = chr;
            text->len++;
            (*cursor)++;
        }
    }
}

void terminal_start() {
    g_terminal_text.ptr = _text_buffer;
    g_terminal_text.max = sizeof(_text_buffer);
    g_terminal_text.len = 0;

    char buffer[50];

    int text_height = 20;
    while (1) {

        // Blocking
        int scancode = ps2_read_scancode();

        int keycode = scancode_to_keycode(scancode);

        if (keycode == KEY_ENTER) {
            g_terminal_cursor_pos = 0;
            g_terminal_text.len = 0;
            g_terminal_text_y += text_height;

            apply_command(STR_CSTR(g_terminal_text));
        } else {
            edit_text(&g_terminal_text, scancode, &g_terminal_cursor_pos);
        }

        //  debug scancode
        snprintf(buffer, sizeof(buffer), "scancode %d", scancode);
        draw_glyphs_from_text_bcolor(500, 500, text_height, PTR_CSTR(buffer), g_default_font, WHITE, DARK_BLUE);

        int text_width = draw_text_width(STR_CSTR(g_terminal_text), text_height, g_default_font);
        draw_glyphs_from_text_bcolor(g_terminal_text_x, g_terminal_text_y, text_height, STR_CSTR(g_terminal_text), g_default_font, WHITE, DARK_BLUE);
        draw_rect(g_terminal_text_x + text_width, g_terminal_text_y, 40, text_height, DARK_BLUE);

        // Cursor
        cstring temp;
        temp.ptr = g_terminal_text.ptr;
        temp.len = g_terminal_cursor_pos;
        int width = draw_text_width(temp, text_height, g_default_font);
        draw_rect(g_terminal_text_x + width, g_terminal_text_y, 2, text_height, WHITE);
    }

}


void apply_command(cstring text) {

    // Parse command

    root := "/home/user"

    list_files(root)

}

