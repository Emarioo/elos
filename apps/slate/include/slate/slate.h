/*
    Crude text editor.

    Slate as in a flat rock you can write on.
*/

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

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
void slate_save(SlateSession* session);

void _start();
