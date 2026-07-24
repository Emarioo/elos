#pragma once

// We assume 64-bit system

#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdbool.h>

typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t  u8;

typedef int64_t s64;
typedef int32_t s32;
typedef int16_t s16;
typedef int8_t  s8;

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
    u8*    ptr;
    size_t len;
} bytearray;


void kernel_bug();

#define min(X,Y) ( (X) < (Y) ? (X) : (Y) )
#define max(X,Y) ( (X) > (Y) ? (X) : (Y) )

#define ARRAY_LENGTH(ARR) ((int)(sizeof(ARR)/sizeof(*ARR)))

#ifdef _WIN32
#define _align(N) __declspec(align(4096))
#else
#define _align(N) __attribute__ ((aligned(4096)))
#endif


#define STALL while (1) pause();
