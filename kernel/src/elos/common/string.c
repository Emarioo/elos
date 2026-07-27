
#include "elos/common/string.h"



int snprintf(char* buffer, size_t size, const char* format, ...) {
    va_list va;
    va_start(va, format);
    const int res = vsnprintf(buffer, size, format, va);
    va_end(va);
    return res;
}




long strtol(const char* ptr, char** endptr, int base) {

    #define isdigit(C) ( (C) >= '0' && (C) <= '9' )

    int head = 0;
    long acc = 0;
    int isNegative = 0;
    int len = strlen(ptr);
    if (base != 10 && base != 8 && base != 16)
        base = 10;

    if (ptr[head] == '-') {
        isNegative = 1;
        head++;
    }

    while (head < len) {
        char c = ptr[head];
        if (base == 8 && c >= '0' && c <= '7') {
            acc = acc * base + c - '0';
            head++;
            continue;
        } else if (base == 10 && c >= '0' && c <= '9') {
            acc = acc * base + c - '0';
            head++;
            continue;
        } else if (base == 16) {
            if (c >= '0' && c <= '9') {
                acc = acc * base + c - '0';
                head++;
                continue;
            } else if ((c|32) >= 'a' && (c|32) <= 'f') {
                acc = acc * base + c - 'a';
                head++;
                continue;
            }
        }
        break;
    }
    if (endptr) {
        *endptr = (char*)ptr + head;
    }
    if (isNegative) {
        acc = -acc;
    }

    return acc;
}




size_t strlen(const char* ptr) {
    const char* base = ptr;
    while(*(ptr++)) ;
    return (u64)ptr - (u64)base - 1;
}
size_t strnlen(const char* ptr, size_t maxlen) {
    int index = 0;
    while(1) {
        if (index >= maxlen)
            return maxlen;
        char chr = ptr[index];
        if (chr == '\0')
            break;
        index++;
    }
    return index;
}

// void memcpy(void* dst, const void* src, int size) {
//     if (dst == src)
//         return;
//     for (int i=0;i<size;i++) {
//         ((char*)dst)[i] = ((char*)src)[i];
//     }
// }

void* memmove(void* dst, const void* src, size_t size) {
    if (dst == src)
        return dst;
    
    if ((u64)dst % 8 == 0 && (u64)src % 8 == 0 && size % 8 == 0) {
        // aligned
        if (dst < src) {
            for (int i=0;i<(int)size/8;i++) {
                ((u64*)dst)[i] = ((u64*)src)[i];
            }
        } else {
            for (int i=(int)size/8-1;i>=0;i--) {
                ((u64*)dst)[i] = ((u64*)src)[i];
            }
        }
    } else {
        if (dst < src) {
            for (int i=0;i<(int)size;i++) {
                ((char*)dst)[i] = ((char*)src)[i];
            }
        } else {
            for (int i=(int)size-1;i>=0;i--) {
                ((char*)dst)[i] = ((char*)src)[i];
            }
        }
    }
    return dst;
}

int memcmp(const void* dst, const void* src, size_t size) {
    for(int i=0;i<size;i++) {
        if ( ((char*)dst)[i] != ((char*)src)[i] )
            // Did i flip the subtraction?
            return ((char*)dst)[i] - ((char*)src)[i];
    }
    return 0;
}

int strcmp(const char* dst, const char* src) {
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

int strncmp(const char* dst, const char* src, size_t len) {
    int i=0;
    while (i < len) {
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

void* memset(void* dst, int val, size_t size) {
    for(size_t i = 0; i < size; i++) {
        *((char*)dst + i) = val;
    }
    return dst;
}


u16* tmp_path_wstring(const char* str) {
    static u16 wstr[256];
    int len = strlen(str);
    for (int i = 0; i < len && i < 256-1; i++) {
        wstr[i] = str[i];
    }
    wstr[len] = 0;
    return wstr;
}


char *strstr(const char *haystack, const char *needle)
{
    if (*needle == 0)
        return (char *)haystack;

    while (*haystack)
    {
        const char *h = haystack;
        const char *n = needle;

        while (*h && *n && *h == *n)
        {
            h++;
            n++;
        }

        if (*n == 0)
            return (char *)haystack;

        haystack++;
    }

    return NULL;
}

char *strrchr(const char *s, int c)
{
    char *last = NULL;

    while (*s)
    {
        if (*s == (char)c)
            last = (char *)s;

        s++;
    }

    if (c == 0)
        return (char *)s;

    return last;
}
char *strncpy(char *dst, const char *src, size_t n)
{
    size_t i;

    for (i = 0; i < n && src[i]; i++)
        dst[i] = src[i];

    for (; i < n; i++)
        dst[i] = 0;

    return dst;
}

char *strchr(const char *s, int c)
{
    while (*s)
    {
        if (*s == (char)c)
            return (char *)s;

        s++;
    }

    if (c == 0)
        return (char *)s;

    return NULL;
}
