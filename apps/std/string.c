
#include "stdint.h"
#include "stddef.h"

#include "stdlib.h"

size_t strlen(const char* s);
void* malloc(size_t size);
void memcpy(void* dst, const void* src, size_t size);


char *strdup(const char *s)
{
    size_t len = strlen(s);
    char *out = malloc(len + 1);

    if (out == NULL)
        return NULL;

    memcpy(out, s, len + 1);
    return out;
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

char *strncpy(char *dst, const char *src, size_t n)
{
    size_t i;

    for (i = 0; i < n && src[i]; i++)
        dst[i] = src[i];

    for (; i < n; i++)
        dst[i] = 0;

    return dst;
}
int strcasecmp(const char *a, const char *b)
{
    while (*a && *b)
    {
        int ca = tolower(*a);
        int cb = tolower(*b);

        if (ca != cb)
            return ca - cb;

        a++;
        b++;
    }

    return tolower(*a) - tolower(*b);
}
int strncasecmp(const char *a, const char *b, size_t n)
{
    while (n && *a && *b)
    {
        int ca = tolower(*a);
        int cb = tolower(*b);

        if (ca != cb)
            return ca - cb;

        a++;
        b++;
        n--;
    }

    if (n == 0)
        return 0;

    return tolower(*a) - tolower(*b);
}

