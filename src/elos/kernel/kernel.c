/*
    
*/

#include "elos/kernel/frame/frame.h"


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

    draw_rect(0, 0, 100, 100, xorshift32());

    while (1) {
        int x = ((u64)xorshift32() * (u64)width) / 0xFFFFFFFFu;
        int y = ((u64)xorshift32() * (u64)height) / 0xFFFFFFFFu;
        int w = (((u64)xorshift32() + 20) * 200u) / 0xFFFFFFFFu;
        int h = (((u64)xorshift32() + 20) * 200u) / 0xFFFFFFFFu;

        draw_rect(x, y, w, h, xorshift32());
    }

}

