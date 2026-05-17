#pragma once

// We assume 64-bit system

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef unsigned long long u64;
typedef unsigned int       u32;
typedef unsigned short     u16;
typedef unsigned char      u8;

typedef long long         s64;
typedef int               s32;
typedef short             s16;
typedef char              s8;

typedef struct cstring {
    const char* ptr;
    u32  len;
} cstring;

typedef struct string {
    char* ptr;
    u32  len;
    u32  max;
} string;

typedef struct bytearray {
    char* ptr;
    u64  len;
} bytearray;


void kernel_bug();

#define min(X,Y) ( (X) < (Y) ? (X) : (Y) )
#define max(X,Y) ( (X) < (Y) ? (Y) : (X) )

#define ARRAY_LENGTH(ARR) (sizeof(ARR)/sizeof(*ARR))

#ifdef _WIN32
#define _align(N) __declspec(align(4096))
#else
#define _align(N) __attribute__ ((aligned(4096)))
#endif
