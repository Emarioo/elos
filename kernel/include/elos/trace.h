#pragma once

#include "elos/common/types.h"

typedef enum {
    TRACE_MEMORY,
    TRACE_DISK,
    TRACE_FILE_SYSTEM,
    TRACE_PROCESS,
    TRACE_SYSCALL,
    TRACE_ASYNC,
    
    TRACE_COUNT,
} Trace_Category;


// Macro so we can remove it completely if we wish
#define TRACEF(CATEGORY, ...) tracef(CATEGORY, __VA_ARGS__)
// #define TRACEF(CATEGORY, ...)


extern bool g_traceEnabled[TRACE_COUNT];


void init_trace();

void tracef(Trace_Category category, const char* format, ...);
