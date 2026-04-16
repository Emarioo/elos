#pragma once

// #include <stdlib.h>
// #include <string.h>
// #include <stdio.h>
#include <stdbool.h>

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long int u64;

typedef char i8;
typedef short i16;
typedef int i32;
typedef long int i64;

struct string {
    int len;
    char* ptr;
};
