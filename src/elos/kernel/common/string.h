#pragma once

#include <stdarg.h>
#include "elos/kernel/common/types.h"

int snprintf(char* buffer, int size, char* format, ...);
int vsnprintf(char* buffer, int size, char* format, va_list va);

static inline int strlen(const char* ptr) {
    const char* base = ptr;
    while(*(ptr++)) ;
    return (u64)ptr - (u64)base - 1;
}
static inline void memcpy(void* dst, const void* src, int size) {
    // char* d = dst;
    // const char* s = src;
    // char* e = d + size;
    // while (d != e) {
    //     *(d++) = *(s++);
    // }
    for (int i=0;i<size;i++) {
        ((char*)dst)[i] = ((char*)src)[i];
    }
}


static inline void memset(void* dst, int val, int size) {
    for(int i=0;i<size;i++) {
        *((char*)dst + i) = val;
    }
}


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

