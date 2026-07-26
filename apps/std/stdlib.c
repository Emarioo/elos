
#include "elos/common/string.h"

#include "elos/syscalls.h"

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdarg.h>
#include <errno.h>

#include <stdio.h>
// #include <sys/stat.h>
// #include <stdlib.h>


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

void exit(int code)
{
    SYS_debug_log("EXIT\n", 5);
    for (;;)
        ;
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
    return c;
}



typedef u32 mode_t;

const unsigned short int** __ctype_b_loc(void) {
    static unsigned short table[256];
    static const unsigned short* ptr = table;

    // TODO: Populate classification table.
    return &ptr;
}

int remove(const char* path) {
    printf("remove(\"%s\")\n", path);
    errno = ENOSYS;
    return -1;
    // @TODO Remove file
    // return 0;
}

int rename(const char* oldpath, const char* newpath) {
    printf("rename(\"%s\", \"%s\")\n", oldpath, newpath);
    errno = ENOSYS;
    return -1;
    // @TODO Move file
    // return 0;
}

int fflush(FILE* stream) {
    (void)stream;
    return 0;
}

int system(const char* command) {
    printf("system(\"%s\")\n", command ? command : "(null)");
    errno = ENOSYS;
    return -1;
}

int sscanf(const char* str, const char* format, ...) {
    printf("sscanf(\"%s\", \"%s\")\n", str, format);
    errno = ENOSYS;
    return 0;
}

double atof(const char* str) {
    printf("atof(\"%s\")\n", str);
    return 0.0;
}

int mkdir(const char* path, mode_t mode) {
    printf("mkdir(\"%s\", %o)\n", path, (unsigned)mode);
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
