#include "elos/kernel/frame/font/font.h"

#include "elos/kernel/frame/font/psf.h"

#define be16(X) __builtin_bswap16(X)
#define be32(X) __builtin_bswap32(X)
#define be64(X) __builtin_bswap64(X)

Font* g_default_font;

#define log(...) printf(__VA_ARGS__)
// #define log(...)

bool font__load_from_bytes(const u8* data, u32 size, Font** out_font) {
    bool res;

    res = font_psf__load_from_bytes(data, size, out_font);
    if (res) {
        return true;
    }

    return false;
}

const Glyph* font__get_glyph(const Font* font, const u32 codepoint) {
    // Quick ascii check
    if (codepoint < 128 && codepoint < font->glyphs_len) {
        return &font->glyphs[codepoint];
    }
    
    // Expensive check
    // we can implement hash map in the future.
    return NULL;
}

bool font__has_codepoint(const Font* font, u32 codepoint) {
    return font__get_glyph(font, codepoint) != NULL;
}
