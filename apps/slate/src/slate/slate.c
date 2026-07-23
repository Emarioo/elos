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

// void draw_rect(int x, int y, int w, int h, uint32_t rgba);

// #define BLACK 0xFF000000
// #define RED 0xFFD91938

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



void editor_loop() {

    bool res = slate_load_font();
    if (!res) {
        exit(1);
    }

    slate_open(&slateSession, "/boot/TEMPLATE.CFG");

    int x = 10;
    int y = 10;
    int size = 20;
    int padding = 2;
    int velx = 1;
    int vely = 1;

    int text_height = 20;
    int text_color = WHITE;

    while (1) {
        ELOS_UserEvent event;
        bool has = get_event(&event);
        if (has) {
            printf("type=%d id=%d chr=%c pressed=%d\n", event.type, event.id, event.key.character, event.key.value);
        }

        draw_rect(0, 0, g_surfaceInfo.width, g_surfaceInfo.height, BACKGROUND);

        for (int li=0;li<slateSession.lines_len;li++) {
            Line* line = &slateSession.lines[li];

            cstring text = { .ptr = line->text, .len = line->length };

            int textWidth = draw_text_width(text, text_height, g_default_font);

            if (textWidth > g_surfaceInfo.width ) {
                text.len = (g_surfaceInfo.width) / (textWidth/text.len);
            }

            draw_glyphs_from_text_bcolor(0, text_height * li, text_height, text, g_default_font, text_color, 0);
        }


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
            // if (len > 0) {
            Line* nextLine = &session->lines[session->lines_len];
            session->lines_len++;
            nextLine->text = buffer + lineStart;
            nextLine->capacity = 0;
            nextLine->length = len;
            // NOT null terminated!
            // }
            lineStart = head;
        }
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


// void draw_rect(int x, int y, int w, int h, uint32_t rgba) {
//     if (x < 0) {
//         w += x;
//         x = 0;
//     }
//     if (y < 0) {
//         h += y;
//         y = 0;
//     }
//     if (x + w > g_surfaceInfo.width)
//         w = g_surfaceInfo.width - x;
//     if (y + h > g_surfaceInfo.height)
//         h = g_surfaceInfo.height - y;

//     uint32_t* const pixels           = g_surfaceInfo.buffer;
//     uint32_t  const pixels_per_line  = g_surfaceInfo.stride;
//     for (int iy = y; iy < y + h; iy++) {
//         for (int ix = x; ix < x + w; ix++) {
//             pixels[ix + iy * pixels_per_line] = rgba;
//         }
//     }
// }




void sleep(u64 ns) {
    u64 start = rdtsc();
    // printf("tps=%d K  start=%d\n", ticks_per_second / 1000, start/1000);
    while (1) {
        u64 now = rdtsc();
        u64 now_ns = (1000000000 * (now - start)) / ticks_per_second;
        if (now_ns > ns) {
            return;
        }
        // printf("tps=%d K  start=%d\n", ticks_per_second / 1000, (now-start)/1000);
    }
}
