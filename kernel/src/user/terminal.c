/*
    Main terminal

    Terminal app
        scrolling
        history
        keyboard input

    Shell executor
        start processes
            printed output sent to terminal, stdout handle.
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

// int ring_size = 0x10000;
// Handle ringBuffer;

#define printf(...) KCON_printf(__VA_ARGS__)


void work();

void terminal_main() {

    printf("Starting terminal\n");
    
    work();
}

typedef struct {
    char text[200];
} Line;

#define LINE_LIMIT 10000
Line lines[1000];
int lines_len;
int lineStart;
int lineScroll;

Line commandHistory[200];
int commandHistory_len;

Line inputBuffer;
int inputBuffer_len;

int g_terminal_cursor_pos;
int g_terminal_x;
int g_terminal_y;
int g_terminal_width;
int g_terminal_height;

int fontHeight = 16;


#define TERMINAL_BACK  0xFF053612

int textColor = WHITE;
int backColor = TERMINAL_BACK;

void send_command(cstring text);


void work() {

    int screenWidth, screenHeight;
    draw_frame_info(&screenWidth, &screenHeight);

    g_terminal_height = 800;
    g_terminal_width = screenHeight - 20;
    g_terminal_x = screenWidth - g_terminal_width;
    g_terminal_y = 10;

    snprintf(inputBuffer.text, sizeof(inputBuffer.text), "> ");
    inputBuffer_len = 2;

    #define REFRESH_RATE (1000000000LU / 60LU)

    while (1) {

        // Poll keyboard events
        // Edit input buffer based on key events.
        KeyEvent keyEvent;
        while (1) {
            bool has = KBD_poll_key_event(&keyEvent);
            if (!has)
                break;
            
            if (keyEvent.keycode == KEY_ENTER && keyEvent.pressed) {
                // First two characters are "> "
                cstring cmd = { inputBuffer.text+2, inputBuffer_len };
                send_command(cmd);
                inputBuffer_len = 2;
                inputBuffer.text[inputBuffer_len] = 0;
            } else if (keyEvent.character && keyEvent.pressed) {
                if (inputBuffer_len+1 < sizeof(inputBuffer.text)) {
                    inputBuffer.text[inputBuffer_len] = keyEvent.character;
                    inputBuffer_len++;
                    inputBuffer.text[inputBuffer_len] = 0;
                }
            }
        }
        

        // Call shell executor (if ENTER and input text is non-empty)

        // If shell executor creates processes then we give them a handle to a PIPE (stdout).
        // Prints they do end up in the PIPE and we read it and put it into
        // our terminal line buffers.

        // printf("%d \n", inputBuffer_len, );

        // Render terminal (input and line buffers)

        int maxLines = g_terminal_height / fontHeight;
        int lineIndex = (lineStart + lineScroll) % LINE_LIMIT;
        while (lineIndex < lines_len) {
            Line* line = &lines[lineIndex];
            cstring lineText = PTR_CSTR(line->text);
            draw_glyphs_from_text_bcolor(g_terminal_x, g_terminal_y + fontHeight * lineIndex, fontHeight, lineText, g_default_font, textColor, backColor);

            lineIndex++;
        }

        cstring lineText = PTR_CSTR(inputBuffer.text);
        // printf("lineText %x %d\n", lineText.ptr, lineText.len);
        draw_rect(g_terminal_x, g_terminal_y + fontHeight * lines_len, fontHeight * 150, fontHeight, backColor);
        draw_glyphs_from_text_bcolor(g_terminal_x, g_terminal_y + fontHeight * lines_len, fontHeight, lineText, g_default_font, textColor, backColor);

        CPU_sleep(REFRESH_RATE);

    }
}


// char _text_buffer[4096];




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


void respond_message(cstring text) {
    Line* nextLine = &lines[lines_len];
    lines_len++;
    int cap = text.len < sizeof(nextLine->text)-1 ? text.len : sizeof(nextLine->text)-1;
    // @TODO Split newlines in text
    memcpy(nextLine->text, text.ptr, cap);
}


void send_command(cstring text) {

    // Shell executor

    if (!strcmp(text.ptr, "cd")) {
        respond_message(PTR_CSTR("No file system\n"));
    } else if (!strcmp(text.ptr, "ls")) {
        respond_message(PTR_CSTR("No file system\n"));
    } else {
        respond_message(PTR_CSTR("Unknown command\n"));
    }
}

