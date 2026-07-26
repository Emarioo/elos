#pragma once

#include <stdint.h>
#include <stddef.h>
#include "elos/syscalls.h"


void* malloc(size_t size);
void  free(void* ptr);
void* realloc(void* ptr, size_t size);
void* calloc(size_t count, size_t elementSize);

int toupper(int c);
int tolower(int c);

int atoi(const char *s);
double atof(const char *str);

void exit(int code);

int puts(const char *s);
int putchar(int c);

void sleep(uint64_t ns);

