/*
    Basic graphics
*/

#include "elos/kernel/video/frame.h"
#include "elos/common/types.h"
#include "elos/common/string.h"
#include "elos/frame_buffer.h"
#include "elos/physical_memory.h"
#include "elos/kernel_console.h"

#define printf(...) KCON_printf(__VA_ARGS__)

#define STBI_NO_STDIO
#include "vendor/stb_image.h"

#include "elos/vfs.h"

    #define PixelBlueGreenRedReserved8BitPerColor 0
    #define PixelRedGreenBlueReserved8BitPerColor 1
    #define PixelBitMask 2
    #define PixelBltOnly 3
    #define PixelFormatMax 4

// #define ascii_width 16;
// #define ascii_height 64;
// extern const u32 ascii_bitmap_width;
// extern const u32 ascii_bitmap_height;
// extern const u32 ascii_bitmap[0];

// void kernel_init_frame() {
//     kernel__core_data->graphics_output.Mode.
// }

void draw_frame_info(int* width, int* height) {
    // TODO: Validate user addresses
    *width = g_frame_buffer.width;
    *height = g_frame_buffer.height;
}

// void draw_text(int x, int y, int h, string text) {
//     EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE* const mode = kernel__core_data->graphics_output->Mode;

//     buffer

//     kernel__core_data->graphics_output->Blt(kernel__core_data->graphics_output, ,
// }
// void draw_char_bcolor(int x, int y, int height, char c, u32 color, u32 back_color) {
//     // EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE* const mode = kernel__core_data->graphics_output->Mode;

//     int format = 0;


//     int w = 8;
//     int h = 8;
//     if (x < 0) {
//         w += x;
//         x = 0;
//     }
//     if (y < 0) {
//         h += y;
//         y = 0;
//     }
//     if (x + w > g_frame_buffer.width)
//         w = g_frame_buffer.width - x;
//     if (y + h > g_frame_buffer.height)
//         h = g_frame_buffer.height - y;

//     const int FACTOR = ((7+height) / 8);

//     switch(format) {
//         case PixelRedGreenBlueReserved8BitPerColor: {
//             // TODO: FIX
//             // color = ((color >> 16) & 0xFF) |
//             //                 ((color << 16) & 0xFF0000) |
//             //                 ((color      ) & 0xFF00FF00); // keep green and alpha (alpha part is reserved and not used though)
//         }
//         // fallthrough
//         case PixelBlueGreenRedReserved8BitPerColor: {
//             // TODO: SIMD
//             u32* const pixels           = (u32*)g_frame_buffer.base;
//             u32  const pixels_per_line  = g_frame_buffer.pixels_per_scan_line;
//             const int dst_offset = x + y * pixels_per_line;
//             const int src_offset = c * 8*8; // each character is 8x8 pixels
//             for (int iy = 0; iy < h; iy++) {
//                 for (int ix = 0; ix < w; ix++) {
//                     u32 pixel = ascii_bitmap[src_offset + ix + iy * 8];
//                     // mix pixel and color?
//                     u32 mix = pixel ? color : back_color;
//                     if (pixel || (ALPHA_MASK & back_color)) {
//                         for (int b=0;b<FACTOR*FACTOR;b++) {
//                             pixels[dst_offset + (FACTOR*ix + b%FACTOR) + (FACTOR*iy + b/FACTOR) * pixels_per_line] = mix;
//                         }
//                     }
//                 }
//             }
//         }
//         break; case PixelBitMask: {
//             // TODO: implement
//         }
//         break; case PixelBltOnly: {
//             // TODO: implement
//         }
//         break; case PixelFormatMax: // do nothing
//     }
// }


int draw_text_width(cstring text, int height, Font* font) {
    return (text.len * font->glyphWidth * height) / font->glyphHeight;
}

static Font g_tempFont = { .format = FONT_FORMAT_NONE, .glyphWidth = 8, .glyphHeight = 8, .glyphs_len = 0, .glyphs = NULL };
// void draw_text_bcolor(int x, int y, int h, cstring text, u32 color, u32 back_color) {
//     int w = draw_text_width(text, h, &g_tempFont);
//     for(int i=0;i<text.len;i++) {
//         draw_char_bcolor(x + w/text.len * i, y, h, text.ptr[i], color, back_color);
//     }
// }

void draw_rect(int x, int y, int w, int h, u32 rgba) {
    if (x < 0) {
        w += x;
        x = 0;
    }
    if (y < 0) {
        h += y;
        y = 0;
    }
    if (x + w > g_frame_buffer.width)
        w = g_frame_buffer.width - x;
    if (y + h > g_frame_buffer.height)
        h = g_frame_buffer.height - y;

    u32 color = rgba;
    switch(0) {
        case PixelRedGreenBlueReserved8BitPerColor: {
            // TODO: FIX
            // color = ((rgba >> 16) & 0xFF) |
            // ((rgba << 16) & 0xFF0000) |
            // ((rgba      ) & 0xFF00FF00); // keep green and alpha (alpha part is reserved and not used though)
        } 
        // fallthrough
        case PixelBlueGreenRedReserved8BitPerColor: {
            // TODO: SIMD
            u32* const pixels           = (u32*)g_frame_buffer.base;
            u32  const pixels_per_line  = g_frame_buffer.pixels_per_scan_line;
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

void draw_shift_frame(int x, int y, u32 fill_color) {
    // EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE* const mode = kernel__core_data->graphics_output->Mode;
    u32* const pixels           = (u32*)g_frame_buffer.base;
    u32  const pixels_per_line  = g_frame_buffer.pixels_per_scan_line;

    // NOTE: Horizontal shift not implemented. This function is mainly for simple scrolling where pixels are lost.

    int abs_y = (y < 0 ? -y : y);

    int total_size = 4 * pixels_per_line * g_frame_buffer.height;
    int shift_size = total_size - 4 * pixels_per_line * abs_y;

    if (y < 0) {
        memmove(pixels, pixels + pixels_per_line * abs_y, shift_size);
        if (fill_color & 0xFF000000) {
            draw_rect(0, g_frame_buffer.height - abs_y, pixels_per_line, abs_y, fill_color);
        }
    } else {
        memmove(pixels + pixels_per_line * abs_y, pixels, shift_size);
        if (fill_color & 0xFF000000) {
            draw_rect(0, 0, pixels_per_line, abs_y, fill_color);
        }
    }
}

extern void serial_write(const char* buffer, int size);

void draw_glyphs_from_text_bcolor(int x, int y, int height, const cstring text, const Font* font, u32 color, u32 back_color) {
    const int pixel_count = g_frame_buffer.pixels_per_scan_line * g_frame_buffer.height;
    int monospace_width  = 1; // determines aspect ratio, we use width and height to avoid floats
    int monospace_height = 2;

    // TODO: Handle rendering out of bounds.
    //   We use some when setting pixel for safety because i don't trust my math.
    //   We should add some up here too for quick check. Check if x,y and width of string is out of bounds.
    //   No need to check individual characters, unless you want too?
    for (int index=0; index < text.len; index++) {
        char chr = text.ptr[index];

        const Glyph* glyph = font__get_glyph(font, chr);
        if (!glyph)
            continue;
        if(glyph->format != GLYPH_FORMAT_GRAYMAP)
            continue; // TODO: Use missing glyph texture

        switch(0) {
            case PixelRedGreenBlueReserved8BitPerColor: {
                // TODO: FIX
                // color = ((color >> 16) & 0xFF) |
                //                 ((color << 16) & 0xFF0000) |
                //                 ((color      ) & 0xFF00FF00); // keep green and alpha (alpha part is reserved and not used though)
            }
            // fallthrough
            case PixelBlueGreenRedReserved8BitPerColor: {
                // TODO: SIMD
                
                u32* const pixels           = (u32*)g_frame_buffer.base;
                u32  const pixels_per_line  = g_frame_buffer.pixels_per_scan_line;

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




Texture* load_texture(const char* path) {

    // @TODO Handle cleanup of allocations if a later one fails.

    VFS_Handle handle = VFS_open(path, VFS_FLAG_READ_ONLY);
    if (!handle) {
        printf("Couldn't open %s\n", path);
        return NULL;
    }

    VFS_HandleInfo info;
    bool yes = VFS_info(handle, &info);
    if (!yes) {
        printf("load_texture: VFS_info failed %s\n", path);
        return NULL;
    }

    u8* data = PMEM_alloc(info.fileSize);
    if (!data) {
        printf("load_texture: Couldn't allocate %d, %s\n", info.fileSize, path);
        return NULL;
    }

    Texture* texture = PMEM_alloc(sizeof(Texture));
    if (!texture)
        return NULL;
    memset(texture, 0, sizeof(*texture));

    u64 readBytes = VFS_read(handle, 0, info.fileSize, data);
    if (readBytes != info.fileSize) {
        printf("Could not load texture, (read %d bytes, texture is %d bytes)\n", readBytes, info.fileSize);
        return NULL;
    }

    printf("STBI parse\n");
    int width, height, channels;
    stbi_uc* rawData = stbi_load_from_memory((stbi_uc*)data, readBytes, &width, &height, &channels, 4); 
    if (!rawData) {
        printf("Could not parse PNG\n");
        return NULL;
    }

    texture->data = (u32*)rawData;
    texture->width = width;
    texture->height = height;

    // @TODO Free unused buffers.

    return texture;
}

void draw_texture(int x, int y, int w, int h, int sub_x, int sub_y, int sub_w, int sub_h, Texture* texture) {
    if (x < 0) {
        w += x;
        x += -x;
        sub_w += (x * sub_w) / w;
        sub_x += (-x * sub_w) / w;
    }
    if (y < 0) {
        h += y;
        y += -y;
        sub_h += (y * sub_h) / h;
        sub_y += (-y * sub_h) / h;
    }
    if (x + w > g_frame_buffer.width) {
        w += -w + g_frame_buffer.width - x;
        sub_w += ((-w + g_frame_buffer.width - x) * sub_w) / w;
    }
    if (y + h > g_frame_buffer.height) {
        h += -h + g_frame_buffer.height - y;
        sub_h += ((-h + g_frame_buffer.height - y) * sub_h) / h;
    }

    switch(0) {
        case PixelRedGreenBlueReserved8BitPerColor: {
            // TODO: FIX
            // color = ((rgba >> 16) & 0xFF) |
            // ((rgba << 16) & 0xFF0000) |
            // ((rgba      ) & 0xFF00FF00); // keep green and alpha (alpha part is reserved and not used though)
        } 
        // fallthrough
        case PixelBlueGreenRedReserved8BitPerColor: {
            // TODO: SIMD
            u32* const pixels           = (u32*)g_frame_buffer.base;
            u32  const pixels_per_line  = g_frame_buffer.pixels_per_scan_line;
            for (int iy = y; iy < y + h; iy++) {
                for (int ix = x; ix < x + w; ix++) {
                    int sx = ((ix - x) * sub_w) / w;
                    int sy = ((iy - y) * sub_h) / h;
                    u32 color_rgba = texture->data[sx + sy * texture->width];
                    u32 color = ((color_rgba >> 16) & 0xFF)
                        | ((color_rgba << 16) & 0xFF0000)
                        | (color_rgba & 0xFF00FF00);

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
