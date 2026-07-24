#pragma once

#include <stdint.h>
#include <stddef.h>

void* malloc(size_t size);
void free(void* ptr);
void* realloc(void* ptr, size_t size);
void* calloc(size_t size);
int toupper(int c);
int tolower(int c);
int atoi(const char *s);
void exit(int code);
int puts(const char *s);
int putchar(int c);

void sleep(uint64_t ns);
