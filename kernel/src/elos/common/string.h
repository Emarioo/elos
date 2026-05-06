#pragma once

#include <stdarg.h>
#include <stdint.h>

#include "elos/common/types.h"

int snprintf(char* buffer, int size, const char* format, ...);
int vsnprintf(char* buffer, int size, const char* format, va_list va);

int strlen(const char* ptr);
void memcpy(void* dst, const void* src, int size);
void memmove(void* dst, const void* src, int size);

int memcmp(void* dst, void* src, int size);

int strcmp(const char* dst, const char* src);
void memset(void* dst, int val, int size);

long strtol(const char* ptr, char** endptr, int base);

static inline cstring STR_CSTR(const string s) {
    cstring st = { s.ptr , s.len };
    return st;
}
static inline cstring PTR_CSTR(const char* s) {
    cstring st = { s, strlen(s) };
    return st;
}

u16* tmp_path_wstring(const char* str);