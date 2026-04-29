#pragma once

#include <stdarg.h>
#include <stdint.h>

#include "elos/common/types.h"

int snprintf(char* buffer, int size, const char* format, ...);
int vsnprintf(char* buffer, int size, const char* format, va_list va);

static inline int strlen(const char* ptr) {
    const char* base = ptr;
    while(*(ptr++)) ;
    return (u64)ptr - (u64)base - 1;
}
static inline void memcpy(void* dst, const void* src, int size) {
    if (dst == src)
        return;
    for (int i=0;i<size;i++) {
        ((char*)dst)[i] = ((char*)src)[i];
    }
}
static inline void memmove(void* dst, const void* src, int size) {
    if (dst == src)
        return;
    
    if ((u64)dst % 8 == 0 && (u64)src % 8 == 0 && size % 8 == 0) {
        // aligned
        if (dst < src) {
            for (int i=0;i<size/8;i++) {
                ((u64*)dst)[i] = ((u64*)src)[i];
            }
        } else {
            for (int i=size/8-1;i>=0;i--) {
                ((u64*)dst)[i] = ((u64*)src)[i];
            }
        }
    } else {
        if (dst < src) {
            for (int i=0;i<size;i++) {
                ((char*)dst)[i] = ((char*)src)[i];
            }
        } else {
            for (int i=size-1;i>=0;i--) {
                ((char*)dst)[i] = ((char*)src)[i];
            }
        }
    }
}

static inline int memcmp(void* dst, void* src, int size) {
    for(int i=0;i<size;i++) {
        if ( ((char*)dst)[i] != ((char*)src)[i] )
            // Did i flip the subtraction?
            return ((char*)dst)[i] - ((char*)src)[i];
    }
    return 0;
}

static inline int strcmp(const char* dst, const char* src) {
    int i=0;
    while (1) {
        char s = src[i];
        char d = dst[i];
        if (d != s) {
            return d - s;
        }
        if (d == 0) {
            break;
        }
        i++;
    }
    return 0;
}

static inline void memset(void* dst, int val, int size) {
    for(int i=0;i<size;i++) {
        *((char*)dst + i) = val;
    }
}

long strtol(const char* ptr, char** endptr, int base);

static inline cstring STR_CSTR(const string s) {
    cstring st = { s.ptr , s.len };
    return st;
}
static inline cstring PTR_CSTR(const char* s) {
    cstring st = { s, strlen(s) };
    return st;
}

static u16* tmp_path_wstring(const char* str) {
    static u16 wstr[256];
    int len = strlen(str);
    for (int i = 0; i < len && i < 256-1; i++) {
        wstr[i] = str[i];
    }
    wstr[len] = 0;
    return wstr;
}

