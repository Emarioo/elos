#pragma once

#include "elos/kernel/common/types.h"
#include "elos/kernel/frame/font/font.h"

// ARGB in big endian (hexidecimal literal)
// BGRA in little endian

#define WHITE      0xFFFFFFFF
#define BLACK      0xFF000000
#define BLUE       0xFF0000FF
#define GREEN      0xFF00FF00
#define RED        0xFFFF0000
#define ALPHA_MASK 0xFF000000

#define DARK_BLUE  0xFF040a18

void draw_frame_info(int* width, int* height);

void draw_char_bcolor(int x, int y, int h, char c, u32 color, u32 back_color);
static inline void draw_char(int x, int y, int h, char c, u32 color)
    { draw_char_bcolor(x,y,h,c,color,0); } // transparent back color 

void draw_text_bcolor(int x, int y, int h, cstring text, u32 color, u32 back_color);
static inline void draw_text(int x, int y, int h, cstring text, u32 color)
    { draw_text_bcolor(x,y,h,text,color, 0); } // transparent back color 

int draw_text_width(cstring text, int height, Font* font);

void draw_rect(int x, int y, int w, int h, u32 rgba);

void draw_glyphs_from_text_bcolor(int x, int y, int height, const cstring text, const Font* font, u32 color, u32 back_color);

void draw_refresh();

void draw_shift_frame(int x, int y, u32 fill_color);
