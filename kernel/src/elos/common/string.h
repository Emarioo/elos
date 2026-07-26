#pragma once

#include "elos/common/types.h"

int snprintf(char* buffer, size_t size, const char* format, ...);
int vsnprintf(char* buffer, size_t size, const char* format, va_list va);

size_t strlen(const char* ptr);
size_t strnlen(const char* ptr, size_t maxlen);
void* memcpy(void* dst, const void* src, size_t size);
void* memmove(void* dst, const void* src, size_t size);

int memcmp(const void* dst, const void* src, size_t size);

int strcmp(const char* dst, const char* src);
int strncmp(const char* dst, const char* src, size_t maxlen);
void* memset(void* dst, int val, size_t size);

long strtol(const char* ptr, char** endptr, int base);

static inline cstring STR_CSTR(const string s) {
    cstring st = { s.ptr , s.len };
    return st;
}
static inline cstring PTR_CSTR(const char* s) {
    cstring st = { s, strlen(s) };
    return st;
}
char *strstr(const char *haystack, const char *needle);
// static inline cstring sub_cstring(const cstring str, int offset) {
//     return (cstring){ str.ptr + offset, str.len - offset };
// }

u16* tmp_path_wstring(const char* str);