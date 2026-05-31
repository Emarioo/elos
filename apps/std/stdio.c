
#include "elos/common/string.h"

#include "elos/syscalls.h"

#include <stdarg.h>

void printf(const char* format, ...) {
    char buffer[256];

    va_list va;
    va_start(va, format);
    int len = vsnprintf(buffer, sizeof(buffer), format, va);
    va_end(va);

    SYS_debug_log(buffer, len);
}
