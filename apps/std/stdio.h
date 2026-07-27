#pragma once

#include "stdint.h"
#include "stddef.h"
#include "stdarg.h"
#include "elos/syscalls.h"

typedef struct FILE FILE;
typedef u32 mode_t;

#define SEEK_SET	0	/* Seek from beginning of file.  */
#define SEEK_CUR	1	/* Seek from current position.  */
#define SEEK_END	2	/* Seek from end of file.  */

extern FILE* stdin;
extern FILE* stderr;
extern FILE* stdout;

int printf(const char* format, ...);
int fprintf(FILE* stream, const char* format, ...);
int vfprintf(FILE* stream, const char* format, va_list args);

int snprintf(char* buffer, size_t size, const char* format, ...);

int fflush(FILE* stream);
int sscanf(const char* str, const char* format, ...);

FILE *fopen(const char *restrict path, const char *restrict mode);
int fclose(FILE *file);

int fseek(FILE *stream, long offset, int whence);
long ftell(FILE *stream);

size_t fread(void* ptr, size_t size, size_t n, FILE *restrict stream);
size_t fwrite(const void* ptr, size_t size, size_t n, FILE *restrict stream);

// Move elos specific elsewhere
ELOS_Error elos_readdir(const char* path, u64* cookie, u64* entryCount, ELOS_DirectoryEntry* buffer);

// Move elos specific elsewhere
char *getcwd(char* buf, size_t size);
int chdir(const char *path);

int mkdir(const char* _path, mode_t mode);
