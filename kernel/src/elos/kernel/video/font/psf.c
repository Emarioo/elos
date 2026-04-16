

#include "elos/kernel/frame/font/psf.h"

#include "elos/kernel/memory/phys_allocator.h"
#include "elos/kernel/common/string.h"

// @TODO: Temporary, needed to print using UEFI
#include <efi.h>
#include <efilib.h>

#define be16(X) __builtin_bswap16(X)
#define be32(X) __builtin_bswap32(X)
#define be64(X) __builtin_bswap64(X)

#define log(...) printf(__VA_ARGS__)
// #define log(...)


static void printf(char* format, ...) {
    char buffer[256];
    unsigned short w_buffer[256];

    va_list va;
    va_start(va, format);
    const int len = vsnprintf(buffer, sizeof(buffer), format, va);
    va_end(va);

    for (int i=0;i<len+1;i++) {
        w_buffer[i] = buffer[i];
    }

    EFI_STATUS status = ST->ConOut->OutputString(ST->ConOut, w_buffer);
    // if(EFI_ERROR(status))
    //     catch_bad_status();
}


#define PSF1_FONT_MAGIC 0x0436
#define PSF1_MODE512    0x01
#define PSF1_MODEHASTAB 0x02
#define PSF1_MODESEQ    0x04 // same as PSF1_MODEHASTAB (https://en.wikipedia.org/wiki/PC_Screen_Font)

typedef struct {
    u16 magic; // Magic bytes for identification.
    u8 fontMode; // PSF font mode.
    u8 characterSize; // PSF character size.
} PSF1_header;


#define PSF2_FONT_MAGIC         0x864ab572
#define PSF2_HAS_unicodeTable 0x1

typedef struct {
    u32 magic;         /* magic bytes to identify PSF */
    u32 version;       /* zero */
    u32 headersize;    /* offset of bitmaps in file, 32 */
    u32 flags;         /* 0 if there's no unicode table */
    u32 numglyph;      /* number of glyphs */
    u32 bytesperglyph; /* size of each glyph */
    u32 height;        /* height in pixels */
    u32 width;         /* width in pixels */
} PSF2_header;



bool font_psf__load_from_bytes(const u8* data, u32 size, Font** out_font) {
    if (size < 4)  return false;

    // @TODO: Fields in Glyph uses unsigned 8-bit integers.
    //   The PSF format may specify characters with larger width and height.
    //   We need to check that and return error if so.

    bool psf2_format;
    int numGlyphs;
    int glyphWidth;
    int glyphHeight;
    int bytesPerGlyph;
    const u8* glyph_data;
    const u8* unicodeTable;
    int unicodeTableSize;

    if (*(u32*)data == PSF2_FONT_MAGIC) {
        
        if (size < sizeof(PSF2_header))
            return false;
        
        const PSF2_header* header = (PSF2_header*)data;
        
        psf2_format   = true;
        numGlyphs     = header->numglyph;
        glyphWidth    = header->width;
        glyphHeight   = header->height;
        bytesPerGlyph = header->bytesperglyph;
        glyph_data    = data + header->headersize;
        
        if (size < header->headersize + numGlyphs * bytesPerGlyph)
            return false;

        if (header->flags & PSF2_HAS_unicodeTable) {
            int offset = header->headersize + numGlyphs * bytesPerGlyph;
            unicodeTable = data + offset;
            unicodeTableSize = size - offset;
        }
        
    } else if (*(u16*)data == PSF1_FONT_MAGIC) {
        
        if (size < sizeof(PSF1_header))
            return false;
        
        const PSF1_header* header = (PSF1_header*)data;
        
        psf2_format   = false;
        numGlyphs     = (header->fontMode & PSF1_MODE512) ? 512 : 256;
        glyphWidth    = 8;
        glyphHeight   = header->characterSize;
        bytesPerGlyph = header->characterSize;
        glyph_data    = data + sizeof(PSF1_header);
        
        if (size < sizeof(PSF1_header) + numGlyphs * bytesPerGlyph)
            return false;

        if(header->fontMode & PSF1_MODEHASTAB) {
            int offset = sizeof(PSF1_header) + numGlyphs * bytesPerGlyph;
            unicodeTable = data + offset;
            unicodeTableSize = size - offset;
        }

    } else {
        // Not PC Screen Font format
        return false;
    }

    // @TODO: Since our Font type don't have a hash table for mapping unicode and Glyphs.
    //   we set limit to the first 256 unicode characters.
    int numGlyphsInFont = numGlyphs;
    if(unicodeTable) {
        numGlyphsInFont = 256;
    }

    int memory_max = sizeof(Font)
        + numGlyphsInFont * sizeof(Glyph)
        + numGlyphsInFont * glyphWidth * glyphHeight;
    // @TODO: Allocate user accessible memory?
    int head_data = 0;
    u8* memory = (u8*)kernel_alloc(memory_max, NULL);
    if (!memory) {
        // @TODO: Error
        return false;
    }

    Font* font = (Font*)(memory + head_data);
    head_data += sizeof(Font);
    
    font->format = FONT_FORMAT_GLYPH;
    font->glyphs_len = numGlyphsInFont;
    font->glyphWidth = glyphWidth;
    font->glyphHeight = glyphHeight;
    font->glyphs = (Glyph*)(memory + head_data);
    head_data += numGlyphsInFont * sizeof(Glyph);
    
    u32 glyphCode = 0;
    int glyphDataIndex = 0;
    int tableHead = 0;
    while (glyphDataIndex < numGlyphs) {
        const u8* bitmap = glyph_data + glyphDataIndex * bytesPerGlyph;
        Glyph* glyph;

        int cur_glyphCode = glyphCode;
        if(!unicodeTable) {
            glyphDataIndex++;
            glyph = &font->glyphs[glyphCode];
            glyphCode++;
        } else {
            glyphDataIndex++;
            glyphCode = -1; // bad glyph

            if (psf2_format) {
                // @TODO: Implement UTF-8
                return false;
            } else {
                bool ignore_sequence = false;
                while (tableHead + 1 < unicodeTableSize) {
                    u16 value = *(u16*)(unicodeTable + tableHead);
                    tableHead += 2;
                    
                    if (value == 0xFFFE) {
                        ignore_sequence = true;
                        continue;
                    }
                    if (value == 0xFFFF) {
                        break;
                    }
                    if(!ignore_sequence) {
                        if(value > 'A' && value < 'Z') {
                            // printf("i:%d v:%d tabl:%d/%d\r\n", (int)glyphDataIndex-1, (int)value, (int)tableHead, (int) unicodeTableSize);
                        }
                        glyphCode = value;
                        ignore_sequence = true; // For now pick the first one, not sure how to handle multiple words
                    }
                }
            }
            
            if (glyphCode == -1) {
                // End of unicode table
                break;
            }

            if (glyphCode >= 256) {
                // @TODO: We don't handle unicodes above 255. Limit in our Font type. We want to improve it's format.
                continue;
            }
            
            glyph = &font->glyphs[glyphCode];
        }

        glyph->format = GLYPH_FORMAT_GRAYMAP;
        glyph->width = glyphWidth;
        glyph->height = glyphHeight;
        glyph->full_width = glyphWidth;
        glyph->full_height = glyphHeight;
        glyph->bearingX = 0;
        glyph->bearingY = 0;
        
        glyph->bitmap = memory + head_data;
        head_data += glyphWidth * glyphHeight;

       

        const int bytes_per_row = (glyphWidth + 7) / 8;
        for (int j = 0; j < glyphWidth * glyphHeight; j++) {
            int col = j % glyphWidth;
            int row = j / glyphWidth;

            u8 bit = 1 & (bitmap[col / 8 + row * bytes_per_row] >> (7 - (col % 8)));
            
            if (bit)
                glyph->bitmap[col + row * glyphWidth] = 0xFF;
            else
                glyph->bitmap[col + row * glyphWidth] = 0;

            // if (glyphCode == 65) {
            //     if (glyph->bitmap[col + row * glyphWidth])
            //         printf("X");
            //     else
            //         printf(" ");
            //     if(col == 7)
            //         printf("\r\n");
            // }
        }
        // if (glyphCode == 65) {
        //     printf("Done\r\n");
        // }
    }

    *out_font = font;
    return true;
}

