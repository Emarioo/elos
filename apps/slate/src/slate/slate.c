/*
    Text editor implementation:

        1. Init compositor connection.

        2. Request a surface/window.

        3. Read keyboard input and perform action (write text and execute command)

        4. Draw text on the surface

        5. Present surface to compositor

        6. Repeat 3
            
*/

#include "slate/slate.h"

#include "prism/prism.h"
#include "elos/syscalls.h"
#include "elos/common/intrinsics.h"
#include "elos/common/string.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "slate/frame.h"


#define ASSERT(expression) ((expression) ? true : (printf("[Assert] %s (%s:%u)\n",#expression,__FILE__,__LINE__), *((char*)0) = 0))

// From apps/std/stdio.c (uses SYS_debug_log)
int printf(const char* format, ...);
void sleep(u64 ns);

void editor_loop();

void line_init(Line* line, char* ptr, int len);


void slate_move(ELOS_Keycode direction);
void slate_insert(char c);
void slate_deletion(ELOS_Keycode direction);


#define BACKGROUND        0xFF0F172A
// #define BACKGROUND_BORDER 0xFFEEEEEE

PrismInstance* g_instance;
PrismSurface* g_surface;

PrismSurfaceInfo g_surfaceInfo;

u64 ticks_per_second;

ELOS_UserEventBuffer* userEvents;

SlateSession slateSession;

bool get_event(ELOS_UserEvent* event) {
    // @TODO Not thread or context switch safe.
    u64 tail = userEvents->tail % userEvents->maxEvents;
    u64 head = userEvents->head % userEvents->maxEvents;
    if (tail == head) {
        return false;
    }
    *event = userEvents->events[tail];
    userEvents->tail++;
    return true;
}

void _start() {

    SYS_ticks_per_second(&ticks_per_second);

    g_instance = prism_init();
    if (!g_instance) {
        printf("slate: Could not init PRISM client\n");
        exit(1);
    }

    g_surface = prism_createSurface(g_instance, 800, 600);
    if (!g_surface) {
        printf("slate: Could not create surface\n");
        exit(1);
    }

    prism_surfaceInfo(g_surface, &g_surfaceInfo);

    prism_moveSurface(g_surface, 200, 200);

    ELOS_Error error;
    error = SYS_request_user_event_buffer(100, &userEvents);
    if (error != ELOS_OK) {
        printf("slate: Could not create user event buffer\n");
        exit(1);
    }

    editor_loop();
}


void* slate_font_allocator(Allocator* allocator, u64 size, void* old_ptr) {
    return realloc(old_ptr, size);
}

bool slate_load_font() {
    const char* path = "/boot/STDFONT.PSF";
    FILE* handle = fopen(path, "rb");
    if (!handle) {
        printf("Couldn't open %s\n", path);
        return false;
    }

    fseek(handle, 0, SEEK_END);
    int fileSize = ftell(handle);
    fseek(handle, 0, SEEK_SET);

    u8* data = malloc(fileSize);
    if (!data) {
        printf("Couldn't allocate %d for %s\n", fileSize, path);
        return false;
    }

    int readBytes = fread(data, 1, fileSize, handle);
    if (readBytes != fileSize) {
        printf("Could not read font %s, (read %d bytes, texture is %d bytes)\n", path, readBytes, fileSize);
        return false;
    }

    Allocator allocator = { slate_font_allocator };
    bool res = font__load_from_bytes(data, fileSize, &g_default_font, &allocator);
    return res;
}

#define LINE_LENGTH(LINE) ((LINE)->text.len)


void apply_numpad(ELOS_Keycode* keycode, int mod) {
    if (!(!(mod & KEY_MOD_NUM_LOCK) ^ !(mod & KEY_MOD_SHIFT))) {
        switch ((int)*keycode) {
            case KEY_NUMPAD_0: *keycode = KEY_INSERT; return;
            case KEY_NUMPAD_1: *keycode = KEY_END; return;
            case KEY_NUMPAD_2: *keycode = KEY_DOWN_ARROW; return;
            case KEY_NUMPAD_3: *keycode = KEY_PAGE_DOWN; return;
            case KEY_NUMPAD_4: *keycode = KEY_LEFT_ARROW; return;
            case KEY_NUMPAD_6: *keycode = KEY_RIGHT_ARROW; return;
            case KEY_NUMPAD_7: *keycode = KEY_HOME; return;
            case KEY_NUMPAD_8: *keycode = KEY_UP_ARROW; return;
            case KEY_NUMPAD_9: *keycode = KEY_PAGE_UP; return;
            case KEY_NUMPAD_COMMA: *keycode = KEY_DELETE; return;
        }
    }
}

void editor_loop() {
    SlateSession* session = &slateSession;

    bool res = slate_load_font();
    if (!res) {
        exit(1);
    }

    slate_open(session, "/boot/TEMPLATE.CFG");

    ASSERT(session->lines_len != 0);

    int x = 10;
    int y = 10;
    int size = 20;
    int padding = 2;
    int velx = 1;
    int vely = 1;

    int text_height = 20;
    int text_color = WHITE;

    int characterWidth;
    {
        cstring temp = PTR_CSTR("hello");
        int tempWidth = draw_text_width(temp, text_height, g_default_font);
        characterWidth = tempWidth / temp.len;
    }

    
    while (1) {
        if (session->lines_len == 0) {
            session->cursor_y = 0;
        } else if (session->cursor_y >= session->lines_len) {
            session->cursor_y = session->lines_len-1;
        }
        Line* cursorLine = &session->lines[session->cursor_y];
        if (session->cursor_x > LINE_LENGTH(cursorLine)) {
            session->cursor_x = LINE_LENGTH(cursorLine);
        }

        ELOS_UserEvent event;
        bool has = get_event(&event);
        if (has && event.type == ELOS_USER_EVENT_KEY && event.key.value == 1) {
            
            apply_numpad(&event.key.keycode, event.key.mods);

            printf("code=%d chr=%c pressed=%d scan=0x%x\n", event.key.keycode, event.key.character, event.key.value, event.key.scancode);

            ELOS_UserEvent_Key key = event.key;
            if (key.keycode == KEY_LEFT_ARROW) {
                slate_move(key.keycode);
            } else if (key.keycode == KEY_RIGHT_ARROW) {
                slate_move(key.keycode);
            } else if (key.keycode == KEY_DOWN_ARROW) {
                slate_move(key.keycode);
            } else if (key.keycode == KEY_UP_ARROW) {
                slate_move(key.keycode);
            } else if (key.keycode == KEY_HOME) {
                slate_move(key.keycode);
            } else if (key.keycode == KEY_END) {
                slate_move(key.keycode);
            } else if (key.keycode == KEY_PAGE_UP) {
                slate_move(key.keycode);
            } else if (key.keycode == KEY_PAGE_DOWN) {
                slate_move(key.keycode);
            } else if (key.keycode == KEY_BACKSPACE) {
                slate_deletion(key.keycode);
            } else if (key.keycode == KEY_DELETE) {
                slate_deletion(KEY_DELETE);
            } else if(key.character != 0) {
                slate_insert(key.character);
            }
        }

        draw_rect(0, 0, g_surfaceInfo.width, g_surfaceInfo.height, BACKGROUND);

        
        for (int li=0;li<slateSession.lines_len;li++) {
            Line* line = &slateSession.lines[li];

            if (LINE_LENGTH(line) == 0)
                continue;

            cstring text = { .ptr = line->text.ptr, .len = LINE_LENGTH(line) };
            
            int textWidth = draw_text_width(text, text_height, g_default_font);
            
            if (textWidth > g_surfaceInfo.width ) {
                text.len = (g_surfaceInfo.width) / characterWidth;
            }
            
            draw_glyphs_from_text_bcolor(0, text_height * li, text_height, text, g_default_font, text_color, 0);
        }
        
        // We required monospace font here
        
        draw_rect(characterWidth * session->cursor_x, text_height * session->cursor_y, 3, text_height, WHITE);

        prism_presentSurface(g_surface);

        // sleep((1000/144)*1000000);
        // printf("HELLO\n");
        sleep((1000/60)*1000000);
    }

}



// #####################
//    EDITOR SPECIFICS
// #####################


void slate_open(SlateSession* session, const char* path) {
    FILE* file;
    char* buffer;

    printf("slate_open: Opening %s\n", path);

    file = fopen(path, "r");
    if (!file) {
        printf("slate_open: Could not open %s\n", path);
        return;
    }
    fseek(file, 0, SEEK_END);
    int fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    buffer = malloc(fileSize + 1);
    if (!buffer) {
        printf("slate_open: Could not allocate %d for %s\n", fileSize, path);
        return;
    }
    
    int readBytes = fread(buffer, 1, fileSize, file);
    if (readBytes != fileSize) {
        printf("slate_open: Could not read %d from %s\n", fileSize, path);
        return;
    }
    buffer[fileSize] = '\0';

    session->cursor_x = 0;
    session->cursor_y = 0;
    session->modified = false;
    strncpy(session->filename, path, sizeof(session->filename)-1);
    session->lines_len = 0;

    if (!session->lines) {
        session->lines_max = 100;
        session->lines = malloc(sizeof(Line) * session->lines_max);
    }

    char* textBuffer = malloc(10000);
    int textBuffer_len = 0;

    int head = 0;
    int lineStart = 0;
    while (head < fileSize) {
        char chr = buffer[head];
        char chr_next = 0;
        if (head + 1 < fileSize)
            chr_next = buffer[head + 1];
        head++;
        
        if (chr == '\n' || (chr == '\r' && chr_next == '\n')) {
            int len = head - lineStart - 1;
            if (chr == '\r') {
                head++;
            }
            Line* nextLine = &session->lines[session->lines_len];
            session->lines_len++;

            line_init(nextLine, buffer + lineStart, len);
            lineStart = head;
        }
    }
    {
        int len = head - lineStart;
        Line* nextLine = &session->lines[session->lines_len];
        session->lines_len++;
        line_init(nextLine, buffer + lineStart, len);
        lineStart = head;
    }

    printf("Lines: %d\n", session->lines_len);

exit:
    // We use buffer memory in line parts so we can't free this.
    // if (buffer) {
    //     free(buffer);
    // }
    if (file) {
        fclose(file);
    }
}

void slate_save(SlateSession* session) {
    printf("slate_save: not implemented\n");
}


// #####################
//    UTILITIES
// #####################



char* textBuffer;
int   textBuffer_len;
int   textBuffer_max;

void reserve_text_buffer(int max) {
    textBuffer = realloc(textBuffer, max);
    textBuffer_max = max;
}

void line_init(Line* line, char* ptr, int len) {
    line->text.ptr = ptr;
    line->text.max = 0;
    line->text.len = len;
}

void slate_move(ELOS_Keycode direction) {
    SlateSession* session = &slateSession;

    int lines_per_pageJump = 10;

    Line* cursorLine = &session->lines[session->cursor_y];

    if (direction == KEY_LEFT_ARROW) {
        if (session->cursor_x > 0) {
            session->cursor_x--;
        }
    } else if (direction == KEY_RIGHT_ARROW) {
        if (session->cursor_x < LINE_LENGTH(cursorLine)) {
            session->cursor_x++;
        }
    } else if (direction == KEY_DOWN_ARROW) {
        if (session->cursor_y < session->lines_len) {
            session->cursor_y++;
        }
    } else if (direction == KEY_UP_ARROW) {
        if (session->cursor_y > 0) {
            session->cursor_y--;
        }
    } else if (direction == KEY_HOME) {
        session->cursor_x = 0;
    } else if (direction == KEY_END) {
        session->cursor_x = LINE_LENGTH(cursorLine);
    } else if (direction == KEY_PAGE_UP) {
        if (session->cursor_y >= lines_per_pageJump) {
            session->cursor_y -= lines_per_pageJump;
        } else {
            session->cursor_y = 0;
        }
    } else if (direction == KEY_PAGE_DOWN) {
        if (session->cursor_y + lines_per_pageJump < session->lines_len) {
            session->cursor_y += lines_per_pageJump;
        } else {
            session->cursor_y = session->lines_len-1;
        }
    } else {
        printf("slate_move: Unhandled keycode %d\n", direction);
        exit(1);
    }
}

void slate_insert(char c) {
    SlateSession* session = &slateSession;
    session->modified = true;

    if (!textBuffer) {
        reserve_text_buffer(10000);
    }

    Line* line = &session->lines[session->cursor_y];


    if (c == '\n') {
        // split line
        ASSERT(session->lines_len < session->lines_max);
        memmove(&session->lines[session->cursor_y+2], &session->lines[session->cursor_y+1], sizeof(Line) * (session->lines_len - (session->cursor_y+1)));
        session->lines_len++;

        Line* splitLine = &session->lines[session->cursor_y+1];
        line_init(splitLine, line->text.ptr + session->cursor_x, line->text.len - session->cursor_x);

        // @TODO What if we split at beginning, end or middle?
        line->text.len = session->cursor_x;
        session->cursor_x = 0;
        session->cursor_y++;
    } else {
        memcpy(textBuffer + textBuffer_len, line->text.ptr, session->cursor_x);
        if (session->cursor_x != line->text.len) {
            memcpy(textBuffer + textBuffer_len + session->cursor_x + 1, line->text.ptr + session->cursor_x, line->text.len - session->cursor_x);
        }
        textBuffer[textBuffer_len + session->cursor_x] = c;
        line->text.ptr = textBuffer + textBuffer_len;
        line->text.len += 1;
        line->text.max = line->text.len;
        textBuffer_len += line->text.len;
        session->cursor_x++;
    }

}

void slate_deletion(ELOS_Keycode direction) {
    SlateSession* session = &slateSession;
    session->modified = true;

    if (!textBuffer) {
        reserve_text_buffer(10000);
    }

    if (direction == KEY_BACKSPACE) {
        if (session->cursor_x > 0) {
            Line* line = &session->lines[session->cursor_y];
            memmove(line->text.ptr + session->cursor_x-1, line->text.ptr + session->cursor_x, line->text.len - session->cursor_x);
            line->text.len--;
            session->cursor_x--;
        } else if (session->cursor_y > 0) {
            Line* prevLine = &session->lines[session->cursor_y-1];
            Line* line = &session->lines[session->cursor_y];
            session->cursor_x = prevLine->text.len;
            memcpy(textBuffer + textBuffer_len, prevLine->text.ptr, prevLine->text.len);
            memcpy(textBuffer + textBuffer_len + prevLine->text.len, line->text.ptr, line->text.len);
            prevLine->text.len += line->text.len;
            prevLine->text.ptr = textBuffer + textBuffer_len;
            prevLine->text.max = prevLine->text.len;
            textBuffer_len += prevLine->text.len;

            memmove(&session->lines[session->cursor_y], &session->lines[session->cursor_y+1], sizeof(Line) * (session->lines_len - session->cursor_y+1));
            session->lines_len--;
            session->cursor_y--;
            printf("Backspace good\n");
        }
    } else if (direction == KEY_DELETE) {
        Line* line = &session->lines[session->cursor_y];
        if (session->cursor_x < line->text.len) {
            memmove(line->text.ptr + session->cursor_x, line->text.ptr + session->cursor_x+1, line->text.len - (session->cursor_x+1));
            line->text.len--;
        } else if (session->cursor_y+1 < session->lines_len) {
            Line* line = &session->lines[session->cursor_y];
            Line* nextLine = &session->lines[session->cursor_y+1];
            memcpy(textBuffer + textBuffer_len, line->text.ptr, line->text.len);
            memcpy(textBuffer + textBuffer_len + line->text.len, nextLine->text.ptr, nextLine->text.len);
            line->text.len += nextLine->text.len;
            line->text.ptr = textBuffer + textBuffer_len;
            line->text.max = line->text.len;
            textBuffer_len += line->text.len;

            memmove(&session->lines[session->cursor_y+1], &session->lines[session->cursor_y+2], sizeof(Line) * (session->lines_len - session->cursor_y+2));
            session->lines_len--;
        }
    } else {
        printf("slate_move: Unhandled keycode %d\n", direction);
        exit(1);
    }
}