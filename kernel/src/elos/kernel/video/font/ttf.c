/*
    TrueType font readering

    WORK IN PROGRESS
*/

#include "elos/kernel/frame/font/font.h"

#define be16(X) __builtin_bswap16(X)
#define be32(X) __builtin_bswap32(X)
#define be64(X) __builtin_bswap64(X)

// Font base;

#define log(...) printf(__VA_ARGS__)
// #define log(...)

typedef u32 Fixed; // 16.16 fixed decimal
typedef s16 FWord;
typedef s64 longDateTime;

typedef struct TTF_OffsetSubtable {
    u32 scalarType;
    u16 numTables;
    u16 searchRange;
    u16 entrySelector;
    u16 rangeShift;
} TTF_OffsetSubtable;

typedef struct TTF_TableEntry {
    // union {
        u32 tag;
        // char _tag_name[4];
    // };
    u32 checkSum;
    u32 offset;
    u32 length;
} TTF_TableEntry;

typedef struct TTF_head {
    Fixed version;
    Fixed fontRevision;
    u32 checkSumAjdustment;
    u32 magicNumber; // 0x5F0F3CF5
    u16 flags;
    u16 unitsPerEm;
    longDateTime created;
    longDateTime modified;
    FWord xMin;
    FWord yMin;
    FWord xMax;
    FWord yMax;
    u16 macStyle;
    s16 lowestRecPPEM;
    s16 fontDirectionHint;
    s16 glyphDataFormat;
} TTF_head;

typedef struct TTF_cmap {
    u16 version;
    u16 numberSubtables;
} TTF_cmap;

typedef struct TTF_cmap_subtable {
    u16 platformID;
    u16 platformSpecificID;
    u32 offset;
} TTF_cmap_subtable;

typedef struct TTF_cmap_format4 {
    u16 format;
    u16 length;
    u16 language;
    u16 segCountX2;
    u16 searchRange;
    u16 entrySelector;
    u16 rangeShift;
    // u16 endCode[segCount];
    // u16 reservedPad;
    // u16 startCode[segCount];
    // u16 idDelta[segCount];
    // u16 idRangeOffset[segCount];
    // u8  glyphIndexArray[256];
} TTF_cmap_format4;

// returns pointer to start of table
// NULL if not found
static void* find_table(const u8* data, char* name) {
    u32 tag = be32(*(u32*)name);
    TTF_OffsetSubtable* offset_table = (TTF_OffsetSubtable*)data;
    TTF_TableEntry* entries = (TTF_TableEntry*)(data + 12);

    for (int i = 0; i < be16(offset_table->numTables); i++) {
        TTF_TableEntry* entry = &entries[i];

        if (be32(entry->tag) == tag) {
            return (void*)(data + be32(entry->offset));
        }
    }
    return NULL;
}

bool font_ttf__load_from_bytes(const u8* data, u32 size, Font** font) {

    TTF_OffsetSubtable* offset_table = (TTF_OffsetSubtable*)data;

    if (be32(offset_table->scalarType) != 0x00010000)
        return false;

    TTF_TableEntry* entries = (TTF_TableEntry*)(data + 12);

    TTF_head* head = (TTF_head*)find_table(data, "head");
    if (!head)
        return false;
    void* loca = find_table(data, "loca");
    if (!loca)
        return false;
    TTF_cmap* cmap = (TTF_cmap*)find_table(data, "cmap");
    if (!cmap)
        return false;
    void* glyf = find_table(data, "glyf");
    if (!glyf)
        return false;

    if (be16(cmap->version) != 0)
        return false;

    TTF_cmap_subtable* cmap_subtables = (TTF_cmap_subtable*)((char*)cmap + sizeof(TTF_cmap));
    TTF_cmap_subtable* unicode_subtable = NULL;
    for (int i=0;i<be16(cmap->numberSubtables);i++) {
        const int Unicode = 0;
        if (be16(cmap_subtables[i].platformID) == Unicode) {
            unicode_subtable = &cmap_subtables[i];
            break;
        }
    }
    if (!unicode_subtable)
        return false;

    TTF_cmap_format4* cmap_format4 = (TTF_cmap_format4*)((char*)cmap + be32(unicode_subtable->offset));

    u16* loca_short = (u16*)loca;
    u32* loca_long = (u32*)loca;

    // base.glyph = 
    // base.glyphs_len = 

    // *font = &base;

    return true;
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
