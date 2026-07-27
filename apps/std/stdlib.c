
#include "elos/common/string.h"
#include "elos/common/intrinsics.h"

#include "elos/syscalls.h"

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdarg.h>
#include <errno.h>
#include <stdio.h>

#include <ctype.h>

#include "async_io.h"

// typedef void FILE;

void* malloc(size_t size) {
    ELOS_Error error;
    void* address;
    error = SYS_heap_allocate(&address, size);
    if (error == ELOS_OK)
        return address;
    return NULL;
}

void free(void* ptr) {
    ELOS_Error error;
    error = SYS_heap_free(ptr);
}

void* realloc(void* ptr, size_t size) {
    ELOS_Error error;
    void* address;
    error = SYS_heap_reallocate(&address, size, ptr);
    if (error == ELOS_OK)
        return address;
    return NULL;
}

void* calloc(size_t count, size_t elementSize) {
    size_t size = count * elementSize;
    ELOS_Error error;
    void* address;
    error = SYS_heap_allocate(&address, size);
    if (error != ELOS_OK)
        return NULL;
    memset(address, 0, size);
    return address;
}

int toupper(int c)
{
    if (c >= 'a' && c <= 'z')
        return c - ('a' - 'A');

    return c;
}
int tolower(int c)
{
    if (c >= 'A' && c <= 'Z')
        return c + ('a' - 'A');

    return c;
}

int atoi(const char *s)
{
    int sign = 1;
    int value = 0;

    if (*s == '-')
    {
        sign = -1;
        s++;
    }

    while (*s >= '0' && *s <= '9')
    {
        value = value * 10 + (*s - '0');
        s++;
    }

    return value * sign;
}

double atof(const char *str) {
    while (isspace((unsigned char)*str))
        str++;

    int sign = 1;
    if (*str == '-') {
        sign = -1;
        str++;
    } else if (*str == '+') {
        str++;
    }

    double value = 0.0;

    while (*str >= '0' && *str <= '9') {
        value = value * 10.0 + (*str - '0');
        str++;
    }

    if (*str == '.') {
        str++;

        double place = 0.1;

        while (*str >= '0' && *str <= '9') {
            value += (*str - '0') * place;
            place *= 0.1;
            str++;
        }
    }

    if (*str == 'e' || *str == 'E') {
        str++;

        int expSign = 1;
        if (*str == '-') {
            expSign = -1;
            str++;
        } else if (*str == '+') {
            str++;
        }

        int exponent = 0;
        while (*str >= '0' && *str <= '9') {
            exponent = exponent * 10 + (*str - '0');
            str++;
        }

        double scale = 1.0;
        while (exponent--) {
            scale *= 10.0;
        }

        if (expSign > 0)
            value *= scale;
        else
            value /= scale;
    }

    return sign * value;
}

void exit(int code) {
    SYS_exit(code);
    printf("SYS_exit SHOULD NOT HAVE RETURNED! spinning...\n");
    while (1) pause(); // spin loop in case we do return by accident? only happens if bug in syscall handler.
     __builtin_unreachable();
}

void sleep(uint64_t ns) {
    SYS_sleep_ns(ns);
}

int puts(const char *s)
{
    SYS_debug_log(s, strlen(s));
    SYS_debug_log("\n", 1);
    return 0;
}
int putchar(int c)
{
    char v = c;
    SYS_debug_log(&v, 1);
    return 0;
}

const unsigned short int** __ctype_b_loc(void) {
    static unsigned short table[256];
    static const unsigned short* ptr = table;

    // TODO: Populate classification table.
    return &ptr;
}



int system(const char* command) {
    printf("system(\"%s\")\n", command ? command : "(null)");
    errno = ENOSYS;
    return -1;
}




int* __errno(void) {
    static int myErrno;
    return &myErrno;
}

int* __errno_location(void) {
    static int myErrno;
    return &myErrno;
}

