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
#include "elos/vfs.h"
#include "elos/physical_memory.h"

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
    if (enabled) {
        disableDefaultMonitorForUsers();
    } else {
        enableDefaultMonitorForUsers();
    }
    g_systemConsole_is_enabled = enabled;
}
bool SCON_is_enabled() {
    return g_systemConsole_is_enabled;
}



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

        if (!g_systemConsole_is_enabled) {
            // Yield process. until next frame.
            EXEC_sleep(REFRESH_RATE);
            continue;
        }
        
        while (hasKeyEvent) {
            if (keyEvent.keycode == ELOSKEY_ENTER && keyEvent.pressed) {
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

        /*
            Rendering to monitor frame buffer requires special care.
            If you draw two different colors on the same area then flicker will occur
            because it is displayed as soon you write to memory (as soon as it can).
            We could write to a temporary buffer then memcpy it to the monitors buffer.
            Since this is slightly slower we don't at the moment.
        */


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
        cstring pretext = PTR_CSTR("> ");
        int pretextWidth = draw_text_width(pretext, fontHeight, g_default_font);


        draw_glyphs_from_text_bcolor(g_terminal_x, text_y, fontHeight, pretext, g_default_font, textColor, backColor);

        int text_x = g_terminal_x + pretextWidth;
        
        int lineTextWidth = draw_text_width(lineText, fontHeight, g_default_font);
        draw_glyphs_from_text_bcolor(text_x, text_y, fontHeight, lineText, g_default_font, textColor, backColor);

        // We cover right part of input text box with background but
        // this means flickering of green and white when we draw cursor on top later.
        int remainingWidth = screenWidth - text_x - lineTextWidth;
        draw_rect(text_x + lineTextWidth, text_y,  remainingWidth, fontHeight, backColor);
        
        cstring temp;
        temp.ptr = inputBuffer.text;
        temp.len = g_terminal_cursor_pos;
        int width = draw_text_width(temp, fontHeight, g_default_font);
        draw_rect(text_x + width, text_y, 2, fontHeight, WHITE);

        EXEC_sleep(REFRESH_RATE);
    }

}


void edit_text(char* text_ptr, int text_max, int* inout_text_len, ELOS_Keycode keycode, int character, int mods, int* cursor) {

    int text_len = *inout_text_len;

    if (keycode == ELOSKEY_LEFT_ARROW) {
        if (*cursor > 0) {
            (*cursor)--;
        }
        // @TODO Handle shift selection and delete.
        //   ctrl+c as well?
    } else if (keycode == ELOSKEY_RIGHT_ARROW) {
        if (*cursor < text_len) {
            (*cursor)++;
        }
    } else if (keycode == ELOSKEY_DELETE) {
        if (text_len <= *cursor ) {
            return;
        }
        memmove(text_ptr + *cursor, text_ptr + *cursor+1, text_len - *cursor - 1);
        text_len--;
    } else if (keycode == ELOSKEY_HOME) {
        *cursor = 0;
    } else if (keycode == ELOSKEY_END) {
        *cursor = text_len;
    } else if (keycode == ELOSKEY_BACKSPACE) {
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
    int head = 0;
    int startHead = 0;
    while (head < text.len) {
        char chr = text.ptr[head];
        head++;

        if (chr == '\n' || head == text.len) {
            Line* nextLine = &lines[lines_len];
            lines_len++;
            
            int len;
            if (chr == '\n')
                len = head-1 - startHead;
            else
                len = head - startHead;

            int cap = len < sizeof(nextLine->text)-1 ? len : sizeof(nextLine->text)-1;
            memcpy(nextLine->text, text.ptr, cap);
        }
    }
}

void printCallback(const char* buffer, size_t size, void* userData) {
    cstring text = { .ptr = buffer, .len = size };
    respond_message(text);
}

ELOS_DirectoryEntry* dirEntries;
int                 dirEntries_cap;

void send_command(cstring text) {

    // Shell executor

    if (!strcmp(text.ptr, "cd")) {
        respond_message(PTR_CSTR("No file system\n"));
    } else if (!strcmp(text.ptr, "ls")) {
        respond_message(PTR_CSTR("No file system\n"));

        // @TODO Implement proper LS

        if (!dirEntries) {
            // @TODO One entry to test the function, don't forget to increase.
            dirEntries_cap = 1;
            dirEntries = PMEM_alloc(dirEntries_cap * sizeof(*dirEntries));
        }

        u64 cookie;
        u64 entryCount;
        
        while (1) {
            entryCount = dirEntries_cap;
            bool yes = VFS_readdir("/", &cookie, &entryCount, dirEntries);
            if (!yes) {
                printf("VFS_readdir: returned false, error\n");
                break;
            }
            char msg[256];
            for (int i=0;i<entryCount;i++) {
                ELOS_DirectoryEntry* entry = &dirEntries[i];
                int len = snprintf(msg, sizeof(msg), "%s\n", entry->name);
                respond_message((cstring){ msg, len });
            }
            if (entryCount != dirEntries_cap) {
                // The end
                break;
            }

        }

    } else if (!strcmp(text.ptr, "mount")) {
        VFS_dump_mounts(printCallback, NULL);
    } else {
        respond_message(PTR_CSTR("Unknown command\n"));
    }
}



