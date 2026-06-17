
#include "elos/common/string.h"

#include "elos/syscalls.h"

#include <stdarg.h>

int printf(const char* format, ...) {
    char buffer[256];

    va_list va;
    va_start(va, format);
    int len = vsnprintf(buffer, sizeof(buffer), format, va);
    va_end(va);

    SYS_debug_log(buffer, len);
}

typedef struct FILE FILE;

FILE* stderr = (FILE*)1;
FILE* stdout = (FILE*)2;


int fprintf(FILE* stream, const char* format, ...) {
    char buffer[256];

    va_list va;
    va_start(va, format);
    int len = vsnprintf(buffer, sizeof(buffer), format, va);
    va_end(va);

    SYS_debug_log(buffer, len);
}



