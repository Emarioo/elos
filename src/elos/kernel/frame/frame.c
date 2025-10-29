/*
    Basic graphics
*/

#include "elos/kernel/frame/frame.h"
#include "elos/kernel/common/types.h"
#include "elos/kernel/common/core_data.h"


#define ascii_width 16;
#define ascii_height 64;
extern const u32 ascii_bitmap_width;
extern const u32 ascii_bitmap_height;
extern const u32 ascii_bitmap[0];

// void kernel_init_frame() {
//     kernel__core_data->graphics_output.Mode.
// }

void draw_frame_info(int* width, int* height) {
    // TODO: Validate user addresses
    *width = kernel__core_data->graphics_output->Mode->Info->HorizontalResolution;
    *height = kernel__core_data->graphics_output->Mode->Info->VerticalResolution;
}

// void draw_text(int x, int y, int h, string text) {
//     EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE* const mode = kernel__core_data->graphics_output->Mode;

//     buffer

//     kernel__core_data->graphics_output->Blt(kernel__core_data->graphics_output, ,
// }
void draw_char_bcolor(int x, int y, int height, char c, u32 color, u32 back_color) {
    EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE* const mode = kernel__core_data->graphics_output->Mode;

    int w = 8;
    int h = 8;
    if (x < 0) {
        w += x;
        x = 0;
    }
    if (y < 0) {
        h += y;
        y = 0;
    }
    if (x + w > mode->Info->HorizontalResolution)
        w = mode->Info->HorizontalResolution - x;
    if (y + h > mode->Info->VerticalResolution)
        h = mode->Info->VerticalResolution - y;

    const int FACTOR = ((7+height) / 8);

    switch(mode->Info->PixelFormat) {
        case PixelRedGreenBlueReserved8BitPerColor: {
            // TODO: FIX
            // color = ((color >> 16) & 0xFF) |
            //                 ((color << 16) & 0xFF0000) |
            //                 ((color      ) & 0xFF00FF00); // keep green and alpha (alpha part is reserved and not used though)
        }
        // fallthrough
        case PixelBlueGreenRedReserved8BitPerColor: {
            // TODO: SIMD
            u32* const pixels           = (u32*)mode->FrameBufferBase;
            u32  const pixels_per_line  = mode->Info->PixelsPerScanLine;
            const int dst_offset = x + y * pixels_per_line;
            const int src_offset = c * 8*8; // each character is 8x8 pixels
            for (int iy = 0; iy < h; iy++) {
                for (int ix = 0; ix < w; ix++) {
                    u32 pixel = ascii_bitmap[src_offset + ix + iy * 8];
                    // mix pixel and color?
                    u32 mix = pixel ? color : back_color;
                    if (pixel || (ALPHA_MASK & back_color)) {
                        for (int b=0;b<FACTOR*FACTOR;b++) {
                            pixels[dst_offset + (FACTOR*ix + b%FACTOR) + (FACTOR*iy + b/FACTOR) * pixels_per_line] = mix;
                        }
                    }
                }
            }
        }
        break; case PixelBitMask: {
            // TODO: implement
        }
        break; case PixelBltOnly: {
            // TODO: implement
        }
        break; case PixelFormatMax: // do nothing
    }
}


int draw_text_width(cstring text, int height) {
    return text.len * ((7+height) / 8) * 8;
}

void draw_text_bcolor(int x, int y, int h, cstring text, u32 color, u32 back_color) {
    int w = draw_text_width(text, h);
    for(int i=0;i<text.len;i++) {
        draw_char_bcolor(x + w/text.len * i, y, h, text.ptr[i], color, back_color);
    }
}

void draw_rect(int x, int y, int w, int h, u32 rgba) {
    EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE* const mode = kernel__core_data->graphics_output->Mode;

    if (x < 0) {
        w += x;
        x = 0;
    }
    if (y < 0) {
        h += y;
        y = 0;
    }
    if (x + w > mode->Info->HorizontalResolution)
        w = mode->Info->HorizontalResolution - x;
    if (y + h > mode->Info->VerticalResolution)
        h = mode->Info->VerticalResolution - y;

    u32 color = rgba;
    switch(mode->Info->PixelFormat) {
        case PixelRedGreenBlueReserved8BitPerColor: {
            // TODO: FIX
            // color = ((rgba >> 16) & 0xFF) |
            // ((rgba << 16) & 0xFF0000) |
            // ((rgba      ) & 0xFF00FF00); // keep green and alpha (alpha part is reserved and not used though)
        } 
        // fallthrough
        case PixelBlueGreenRedReserved8BitPerColor: {
            // TODO: SIMD
            u32* const pixels           = (u32*)mode->FrameBufferBase;
            u32  const pixels_per_line  = mode->Info->PixelsPerScanLine;
            for (int iy = y; iy < y + h; iy++) {
                for (int ix = x; ix < x + w; ix++) {
                    pixels[ix + iy * pixels_per_line] = color;
                }
            }
        }
        break; case PixelBitMask: {
            // TODO: implement
        }
        break; case PixelBltOnly: {
            // TODO: implement
        }
        break; case PixelFormatMax: // do nothing
    }
}

void draw_refresh() {
    // TODO: implement, needed for Blt?
    //   The draw functions set pixels on our in-memory frame buffer.
    //   We then blit it to EFI GRAPHICS OUTPUT protocol?
}


void draw_glyphs_from_text_bcolor(int x, int y, int height, const cstring text, const Font* font, u32 color, u32 back_color) {
    EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE* const mode = kernel__core_data->graphics_output->Mode;
    const int pixel_count = mode->Info->HorizontalResolution * mode->Info->VerticalResolution;
    int monospace_width  = 1; // determines aspect ratio, we use width and height to avoid floats
    int monospace_height = 2;

    // TODO: Handle rendering out of bounds.
    //   We use some when setting pixel for safety because i don't trust my math.
    //   We should add some up here too for quick check. Check if x,y and width of string is out of bounds.
    //   No need to check individual characters, unless you want too?

    for (int index=0; index < text.len; index++) {
        char chr = text.ptr[index];

        const Glyph* glyph = font__get_glyph(font, chr);
        if(glyph->format != GLYPH_FORMAT_GRAYMAP)
            continue; // TODO: Use missing glyph texture

        switch(mode->Info->PixelFormat) {
            case PixelRedGreenBlueReserved8BitPerColor: {
                // TODO: FIX
                // color = ((color >> 16) & 0xFF) |
                //                 ((color << 16) & 0xFF0000) |
                //                 ((color      ) & 0xFF00FF00); // keep green and alpha (alpha part is reserved and not used though)
            }
            // fallthrough
            case PixelBlueGreenRedReserved8BitPerColor: {
                // TODO: SIMD
                
                u32* const pixels           = (u32*)mode->FrameBufferBase;
                u32  const pixels_per_line  = mode->Info->PixelsPerScanLine;

                // HA, good luck understanding this math future me!
                //  It's integer math where we keep precision and are wary of integer division.
                //  You could simplify this with float math.

                int rendered_width  = (glyph->width * height + glyph->full_height - 1 ) / glyph->full_height;
                int rendered_height = (glyph->height * height + glyph->full_height - 1) / glyph->full_height;
                // TODO: This rendered width/height describes the space a whole glyph
                //   can occupy but our bitmaps are slightly smaller so we only
                //   need to render a part.

                int rendered_bearing = (glyph->bearingX * height + glyph->full_height-1)/(glyph->full_height) 
                + ( (glyph->bearingY * height + glyph->full_height - 1) / glyph->full_height ) * pixels_per_line;
                
                int rendered_char_offset = (index * height * monospace_width) / monospace_height;

                const int dst_offset = rendered_char_offset + x + y * pixels_per_line + rendered_bearing;
                for (int iy = 0; iy < rendered_height; iy++) {
                    for (int ix = 0; ix < rendered_width; ix++) {
                        u8 value = glyph->bitmap[
                            (ix * glyph->full_height) / (height) + 
                            ((iy * glyph->full_height) / (height)) * glyph->width
                        ];
                        u32 pixel = value | (value << 8) | (value << 16) | (value << 24);
                        // mix pixel and color?
                        u32 mix = pixel ? color : back_color;
                        if (pixel || (ALPHA_MASK & back_color)) {
                            int ind = dst_offset + ix + iy * pixels_per_line;
                            if (ind >= 0 && ind < pixel_count) {
                                pixels[ind] = mix;
                            }
                        }
                    }
                }
            }
            break; case PixelBitMask: {
                // TODO: implement
            }
            break; case PixelBltOnly: {
                // TODO: implement
            }
            break; case PixelFormatMax: // do nothing
        }
    }
}