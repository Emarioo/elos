/*
    
*/

#include "elos/kernel/frame/frame.h"
#include "immintrin.h"
#include "elos/kernel/common/string.h"


u32 make_color(u8 r, u8 g, u8 b) {
    return (int)r | ((int)g << 8) | ((int)b << 16);
}

static unsigned long xorshift_state = 2463534242;

unsigned int xorshift32(void) {
    unsigned x = xorshift_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return xorshift_state = x;
}

void srandx(unsigned long s) {
    xorshift_state = s ? s : 2463534242;
}

void kernel_entry() {
    

    int width,height;
    draw_frame_info(&width,&height);
    
    // u32 green = make_color(10, 170, 50);
    // u32 white = make_color(255, 255, 255);

    int font_size = 16;
    
    
    draw_glyphs_from_text_bcolor(40, 400, 16, PTR_CSTR("Hello Sailor!"), g_default_font, WHITE, BLACK);

    draw_glyphs_from_text_bcolor(40, 420, 18, PTR_CSTR("Hello Sailor!"), g_default_font, WHITE, BLACK);
    draw_glyphs_from_text_bcolor(40, 440, 20, PTR_CSTR("Hello Sailor!"), g_default_font, WHITE, BLACK);
    draw_glyphs_from_text_bcolor(40, 480, 24, PTR_CSTR("Hello Sailor!"), g_default_font, WHITE, BLACK);
    draw_glyphs_from_text_bcolor(40, 500, 29, PTR_CSTR("Hello Sailor!"), g_default_font, WHITE, BLACK);
    draw_glyphs_from_text_bcolor(40, 540, 30, PTR_CSTR("Hello Sailor!"), g_default_font, WHITE, BLACK);
    draw_glyphs_from_text_bcolor(40, 580, 36, PTR_CSTR("Hello Sailor!"), g_default_font, WHITE, BLACK);
    
    draw_rect(0, 0, 100, 100, xorshift32());

    draw_text_bcolor(520, 550, font_size, PTR_CSTR("Hello Sailor!") , WHITE, BLACK);

    for(int c = 0; c < 128; c++) {
        draw_char(10 + (c%16) * 12, 10 + (c/16) * 12, font_size, c, xorshift32());
    }
    
    while (1) {
        int x = ((u64)xorshift32() * (u64)width) / 0xFFFFFFFFu;
        int y = ((u64)xorshift32() * (u64)height) / 0xFFFFFFFFu;
        // int w = (((u64)xorshift32() + 20) * 200u) / 0xFFFFFFFFu;
        // int h = (((u64)xorshift32() + 20) * 200u) / 0xFFFFFFFFu;
        int r = (((u64)xorshift32()) * (256)) / 0xFFFFFFFFu;
        int g = (((u64)xorshift32()) * (256)) / 0xFFFFFFFFu;
        int b = (((u64)xorshift32()) * (256)) / 0xFFFFFFFFu;

        u32 col = make_color(r,g,b);

        // draw_char(x, y, font_size, c, xorshift32());

        
        // Draw random text
        // draw_glyphs_from_text_bcolor(x, y, 16, PTR_CSTR("Hello Sailor!"), g_default_font, col, 0);


        // _mm_pause();
        // for(int n=0;n<100000;n++) {
        //     x *= x;
        // }
    }

}

