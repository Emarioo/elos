/*
Custom font format.
(using an existing one would be smart but it's nice to have freedom with a custom format)

In a font we want:
- Good scaling, keeping quality while changing font size
- Transparency
- Gray scale, we want blurred and smooth edges
- Font describes a subset of UNICODE glyphs (characters).
- Each glyph has a width and height and a center which is the character line it is placed on.
- When drawing text we can specify monospace and padding between glyphs.
- Rendering text at (x = 10, y = 10, height = 10) will place the glyphs neatly in the 10 height box.
No glyphs are spilling outside. The center line is there for not placed at the complete bottom.
- The data format for glyphs can vary. We support bitmap to begin with (poor scaling quality)

Note that characters are taller than they wide.
*/

#pragma once

#include "elos/kernel/common/types.h"


typedef enum GlyphFormat {
    GLYPH_FORMAT_NONE,
    GLYPH_FORMAT_GRAYMAP, // grayscale/alpha
    // GLYPH_FORMAT_BITMAP, // bit per pixel
    GLYPH_FORMAT_MAX,
} enum_GlyphFormat;
typedef u8 GlyphFormat;

typedef enum FontFormat {
    FONT_FORMAT_NONE,
    FONT_FORMAT_GLYPH,
    FONT_FORMAT_MAX,
} enum_FontFormat;
typedef u8 FontFormat;

typedef struct Glyph {
    GlyphFormat format;
    u8 width;
    u8 height;
    u8 full_width; // maybe saved in Font instead of per glyph?
    u8 full_height;
    u8 bearingX;
    u8 bearingY;
    u8* bitmap;
} Glyph;

typedef struct Font {
    FontFormat format;
    
    // TODO: Hash map of codepoints
    Glyph* glyphs;
    u32 glyphs_len;
} Font;


extern Font* g_default_font;

bool font__load_from_bytes(const u8* data, u32 size, Font** out_font);

const Glyph* font__get_glyph(const Font* font, u32 codepoint);

// unicode codepoint
bool font__has_codepoint(const Font* font, u32 codepoint);


