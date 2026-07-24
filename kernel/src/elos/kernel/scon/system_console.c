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

#include "elos/system_console.h"

#include "elos/keyboard.h"
#include "elos/kernel_console.h"
#include "elos/kernel/video/frame.h"
#include "elos/cpu.h"
#include "elos/execution.h"

#include "elos/common/string.h"
#include "elos/common/intrinsics.h"

#define printf(...) KCON_printf(__VA_ARGS__);

static bool g_systemConsole_is_enabled;


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
void edit_text(char* text_ptr, int text_max, int* inout_text_len, ELOS_Keycode keycode, int character, int mods, int* cursor);

void SCON_enable(bool enabled) {
    g_systemConsole_is_enabled = enabled;
}


ELOS_Keycode g_superKey = KEY_LEFT_ALT;
bool g_superKeyIsDown = false;

/*
    Entry point of the system console.
    A thread should be created since it will never return.
*/
void SCON_main() {
    int screenWidth, screenHeight;
    draw_frame_info(&screenWidth, &screenHeight);

    g_terminal_height = 800;
    g_terminal_width = screenHeight - 20;
    g_terminal_x = screenWidth - g_terminal_width;
    g_terminal_y = 10;

    inputBuffer_len = 0;
    inputBuffer.text[0] = 0;

    printf("SCON main\n");

    #define REFRESH_RATE (1000000000LU / 60LU)

    while (1) {
        
        // Poll keyboard events
        // Edit input buffer based on key events.
        KeyEvent keyEvent;
        bool hasKeyEvent = KBD_poll_key_event(&keyEvent);

        if (hasKeyEvent) {
            if (keyEvent.keycode == g_superKey) {
                g_superKeyIsDown = keyEvent.pressed;
            }
            // printf("%d=%d %d %d %d scan=%d\n", keyEvent.keycode, g_superKey, keyEvent.pressed, keyEvent.mods, g_superKeyIsDown, keyEvent.scancode);
            if (keyEvent.keycode == KEY_T && keyEvent.pressed && g_superKeyIsDown) {
                SCON_enable(!g_systemConsole_is_enabled);
                continue;
            }
        }


        if (!g_systemConsole_is_enabled) {
            // Yield process. until next frame.
            // pause();
            CPU_sleep(REFRESH_RATE);
            continue;
        }
        
        while (hasKeyEvent) {
            if (keyEvent.keycode == KEY_ENTER && keyEvent.pressed) {
                cstring cmd = { inputBuffer.text, inputBuffer_len };
                send_command(cmd);
                inputBuffer_len = 0;
                inputBuffer.text[inputBuffer_len] = 0;
                g_terminal_cursor_pos = 0;
            } else if (keyEvent.pressed) {
                edit_text(inputBuffer.text, sizeof(inputBuffer), &inputBuffer_len, keyEvent.keycode, keyEvent.character, keyEvent.mods, &g_terminal_cursor_pos);
            }

            hasKeyEvent = KBD_poll_key_event(&keyEvent);
        }

        
        int maxLines = g_terminal_height / fontHeight;
        int lineIndex = (lineStart + lineScroll) % LINE_LIMIT;
        while (lineIndex < lines_len) {
            Line* line = &lines[lineIndex];
            cstring lineText = PTR_CSTR(line->text);
            draw_glyphs_from_text_bcolor(g_terminal_x, g_terminal_y + fontHeight * lineIndex, fontHeight, lineText, g_default_font, textColor, backColor);

            lineIndex++;
        }

        int text_y = g_terminal_y + fontHeight * lines_len;
        
        cstring lineText = { inputBuffer.text, inputBuffer_len };
        // printf("lineText %x %d\n", lineText.ptr, lineText.len);
        draw_rect(g_terminal_x, text_y, fontHeight * 150, fontHeight, backColor);
        cstring pretext = PTR_CSTR("> ");
        int pretextWidth = draw_text_width(pretext, fontHeight, g_default_font);


        draw_glyphs_from_text_bcolor(g_terminal_x, text_y, fontHeight, pretext, g_default_font, textColor, 0);

        int text_x = g_terminal_x + pretextWidth;

        draw_glyphs_from_text_bcolor(text_x, text_y, fontHeight, lineText, g_default_font, textColor, 0);

        
        cstring temp;
        temp.ptr = inputBuffer.text;
        temp.len = g_terminal_cursor_pos;
        int width = draw_text_width(temp, fontHeight, g_default_font);
        draw_rect(text_x + width, text_y, 2, fontHeight, WHITE);

        // @TODO We should yield process instead.
        //    We have setup scheduling to run system console loop 60 times per second.
        CPU_sleep(REFRESH_RATE);
    }

}


void edit_text(char* text_ptr, int text_max, int* inout_text_len, ELOS_Keycode keycode, int character, int mods, int* cursor) {

    int text_len = *inout_text_len;

    if (keycode == KEY_LEFT_ARROW) {
        if (*cursor > 0) {
            (*cursor)--;
        }
        // @TODO Handle shift selection and delete.
        //   ctrl+c as well?
    } else if (keycode == KEY_RIGHT_ARROW) {
        if (*cursor < text_len) {
            (*cursor)++;
        }
    } else if (keycode == KEY_DELETE) {
        if (text_len <= *cursor ) {
            return;
        }
        memmove(text_ptr + *cursor, text_ptr + *cursor+1, text_len - *cursor - 1);
        text_len--;
    } else if (keycode == KEY_HOME) {
        *cursor = 0;
    } else if (keycode == KEY_END) {
        *cursor = text_len;
    } else if (keycode == KEY_BACKSPACE) {
        if (*cursor <= 0 ) {
            return;
        } else {
            memmove(text_ptr + *cursor - 1, text_ptr + *cursor, text_len - *cursor);
            (*cursor)--;
            text_len--;
        }
    } else {
        if (character == 0)
            return; // not a char code.
        if (text_len >= text_max) {
            return;
        }
        if (*cursor == text_len) {
            text_ptr[*cursor] = character;
            text_len++;
            (*cursor)++;
            text_ptr[*cursor] = '\0';
        } else {
            memmove(text_ptr + *cursor + 1, text_ptr + *cursor, text_len - *cursor);
            text_ptr[*cursor] = character;
            text_len++;
            (*cursor)++;
        }
    }
    *inout_text_len = text_len;
}

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



