/*
    Basic graphics
*/

#include "elos/kernel/common/types.h"
#include "elos/kernel/common/core_data.h"




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
// void draw_char(int x, int y, char c) {
//     EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE* const mode = kernel__core_data->graphics_output->Mode;

//     buffer

//     kernel__core_data->graphics_output->Blt(kernel__core_data->graphics_output, ,
// }

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
        case PixelBlueGreenRedReserved8BitPerColor: {
            color = ((rgba >> 16) & 0xFF) |
                            ((rgba << 16) & 0xFF0000) |
                            ((rgba      ) & 0xFF00FF00); // keep green and alpha (alpha part is reserved and not used though)
        } 
        // fallthrough
        case PixelRedGreenBlueReserved8BitPerColor: {
            // TODO: SIMD
            u32* const pixels           = (u32*)mode->FrameBufferBase;
            u32  const pixels_per_line  = mode->Info->PixelsPerScanLine;
            for (int i = x; i < x + w; i++) {
                for (int j = y; j < y + h; j++) {
                    pixels[i + j * pixels_per_line] = color;
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

void draw_pixel() {

}

void draw_refresh() {

}
