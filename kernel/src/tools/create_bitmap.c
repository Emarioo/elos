
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

int main(int argc, char** argv) {

    int x,y,channels;
    const char* out_path = "res/ascii_bitmap.c";
    const char* image_path = "res/ascii_bitmap.png";
    char* data = (char*)stbi_load(image_path, &x, &y, &channels, 4);
    if(!data) {
        fprintf(stderr, "Could not find or parse '%s'\n", image_path);
        return 1;
    }

    printf("width/height: %d/%d\n", x, y);

    
    FILE* out = fopen(out_path, "wb");
    if(!data) {
        fprintf(stderr, "Could write to '%s'\n", image_path);
        return 1;
    }

    fprintf(out, "const unsigned int ascii_bitmap_width = %d;\n", x);
    fprintf(out, "const unsigned int ascii_bitmap_height = %d;\n", y);
    fprintf(out, "const unsigned int ascii_bitmap[%d] = {\n", x * y);
    
    unsigned int* pixels = (unsigned int*)data;
    
    const int scanline = 16*8;
    for (int c = 0; c < 128; c++) {
        fprintf(out, "\n// %c, (%d)", c < 32 ? 32 : c, (int)c);
        for (int pi = 0; pi < 64; pi++) {
            int cx = c % 16;
            int cy = c / 16;
            int px = pi % 8;
            int py = pi / 8;
            
            int index = px + py * scanline + cx*8 + cy * 8 * scanline;

            if(pi % 8 == 0)
                fprintf(out, "\n    ");

            fprintf(out, "0x%-8x, /* %5d */", pixels[index], index);
            
        }
    }

    fprintf(out, "\n};\n");

    fclose(out);

    return 0;
}
