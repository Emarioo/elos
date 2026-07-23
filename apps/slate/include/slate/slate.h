/*
    Crude text editor.

    Slate as in a flat rock you can write on.
*/

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct {
    char* text;
    int   length;
    int   capacity;
} Line;

typedef struct {
    Line *lines;
    int   lines_len;
    int   lines_max;

    int cursor_x;
    int cursor_y;

    bool modified;

    char filename[256];
} SlateSession;

void slate_open(SlateSession* session, const char* path);
void slate_save(SlateSession* session);

void _start();
