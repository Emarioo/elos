#pragma once

// We assume 64-bit system

typedef unsigned long long u64;
typedef unsigned int       u32;
typedef unsigned short     u16;
typedef unsigned char      u8;

typedef long long         s64;
typedef int               s32;
typedef short             s16;
typedef char              s8;

typedef unsigned char     bool;

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

#define false 0
#define true 1
#define NULL 0
