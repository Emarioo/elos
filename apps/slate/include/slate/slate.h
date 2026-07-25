/*
    Crude text editor.

    Slate as in a flat rock you can write on.
*/

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "elos/syscalls.h"
#include "elos/common/string.h"

typedef struct {
    string text;
} Line;

typedef enum {
    CMD_NONE,
    CMD_OPEN_FILE,
    CMD_SAVE_FILE,
} SlateCommand;

typedef struct {
    uint32_t color_background;
    uint32_t color_commandBackgroundColor;
    uint32_t color_text;
    uint32_t color_cursor;
    uint32_t color_lineNumber;

    bool showLineNumbers;
    // @TODO Show relative numbers

    // Opacity setting
} SlateConfig;

/*
    One session per process.
*/
typedef struct {
    SlateConfig config;

    Line* lines;
    int   lines_len;
    int   lines_max;

    uint32_t cursor_x;
    uint32_t cursor_y;

    uint32_t scroll_x;
    uint32_t scroll_y;

    bool modified;

    char currentFile[256];

    SlateCommand command;
    uint32_t     commandCursor_x;
    int          commandBuffer_len;
    char         commandBuffer[256];
} SlateSession;

void slate_open(SlateSession* session, const char* path);
void slate_save(SlateSession* session, const char* path);

bool slate_load_font();

void reset_chunks();
void line_init(Line* line, char* ptr, int len);

void slate_move(ELOS_Keycode direction);
void slate_insert(char c);
void slate_deletion(ELOS_Keycode direction);



void _start();

#define ASSERT(expression) ((expression) ? true : (printf("[Assert] %s (%s:%u)\n",#expression,__FILE__,__LINE__), *((char*)0) = 0))

