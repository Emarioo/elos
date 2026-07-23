#pragma once

#include "stdint.h"
#include "stddef.h"

size_t strlen(const char* s);
void* malloc(size_t size);
void memcpy(void* dst, const void* src, size_t size);

// static inline char tolower(char chr) {
//     return chr >= 'A' && chr <= 'Z' ? (chr|32) : chr;
// }

char *strdup(const char *s);
char *strrchr(const char *s, int c);

char *strchr(const char *s, int c);
char *strstr(const char *haystack, const char *needle);

char *strncpy(char *dst, const char *src, size_t n);
int strcasecmp(const char *a, const char *b);

int strncasecmp(const char *a, const char *b, size_t n);
