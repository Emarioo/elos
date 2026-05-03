
#include "elos/frame_buffer.h"

#include "elos/common/types.h"
#include "elos/common/string.h"

#include "elos/kernel/video/frame.h"
#include "elos/kernel/video/font/font.h"

#include <stdarg.h>


FrameBuffer g_frame_buffer;



#define border_padding 20
static int pos_x = border_padding;
static int pos_y = border_padding;




void FB_init(BootAPI* boot_api) {
    g_frame_buffer.base = boot_api->frame_buffer_base;
    g_frame_buffer.size = boot_api->frame_buffer_size;
    g_frame_buffer.width = boot_api->frame_buffer_width;
    g_frame_buffer.height = boot_api->frame_buffer_height;
    g_frame_buffer.pixels_per_scan_line = boot_api->frame_buffer_pixels_per_scan_line;

    // Init font stuff?

}




void FB_printf(const char* format, ...) {
    char buffer[256];

    va_list va;
    va_start(va, format);
    const int len = vsnprintf(buffer, sizeof(buffer), format, va);
    va_end(va);

    FB_write(buffer, len);
}

void FB_write(const char* buffer, int len) {

    int start = 0;
    int head = 0;
    const int text_height = 16;
    while (head < len) {
        bool print = false;
        int end = head;
        char chr = buffer[head];
        head++;

        if (chr == '\r')
            print = true;
        if (chr == '\n')
            print = true;
        if (head >= len)
            print = true;

        if (!print)
            continue;
        
        cstring text = { buffer + start, head - start };
        head++;
        start = head;

        if (text.len > 0) {
            draw_glyphs_from_text_bcolor(pos_x, pos_y, text_height, text, g_default_font, WHITE, 0);
            // draw_glyphs_from_text_bcolor(pos_x, pos_y, text_height, text, g_default_font, WHITE, DARK_BLUE);
            int text_width = draw_text_width(text, text_height, g_default_font);
            pos_x += text_width;
        }
        if (chr == '\r') {
            pos_x = border_padding;
        }
        if (chr == '\n') {
            pos_x = border_padding;
            pos_y += text_height;

            int screen_width, screen_height;
            draw_frame_info(&screen_width, &screen_height);

            // draw_rect(0, pos_y + text_height, screen_width, text_height, DARK_BLUE);

            if (pos_y + text_height + border_padding >= screen_height) {
                // @TODO: When about to go beyond the screen border we wrap around.
                //   We should scroll all text up instead but it's an expensive operation and.
                //   we need to remember the printed text.
                //   Altough maybe we could do a big memmove of the pixel data/frame buffer?
                pos_y = border_padding;

                // draw_shift_frame(0, -text_height, DARK_BLUE);
            }
        }
    }

}
