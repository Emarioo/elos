#pragma once

#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>

#ifdef TRIAL_IMPL
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "elos/elos.h"
#endif


#define MAX_ASSERTS 1024


extern char g_coverageVector[MAX_ASSERTS/8];
extern int g_startCounter;

void _trial_printf(const char* format, ...);

void _trial_assert(const char* name, int cond);
void _trial_start(int startCounter);
void _trial_end(int endCounter);


#define trial_start() _trial_start(__COUNTER__)
#define trial_end() _trial_end(__COUNTER__)

#define trial_assert(NAME, COND) do {                  \
    _trial_assert(NAME, COND);                         \
    int tv_index = __COUNTER__ - g_startCounter;       \
    g_coverageVector[tv_index >> 3] |= 1 << (tv_index & 7);  \
    } while (0)

#ifdef TRIAL_IMPL


int g_totalAsserts;
int g_passedAsserts;
char g_coverageVector[MAX_ASSERTS/8];
int g_startCounter;


void _trial_printf(const char* format, ...) {
    char buffer[256];

    va_list va;
    va_start(va, format);
    const int len = vsnprintf(buffer, sizeof(buffer), format, va);
    va_end(va);
    
    SYS_debug_log(buffer, len);
}

void _trial_assert(const char* name, int cond) {
    if (cond) {
        _trial_printf("[TRIAL] PASS %s\n", name);
        g_passedAsserts++;
    } else {
        _trial_printf("[TRIAL] FAIL %s, %d\n", name, cond);
    }
    g_totalAsserts++;
}
void _trial_start(int startCounter) {
    _trial_printf("[TRIAL] Start\n");
    g_startCounter = startCounter + 1;
    g_totalAsserts = 0;
    g_passedAsserts = 0;
    memset(g_coverageVector, 0, sizeof(g_coverageVector));
}
void _trial_end(int endCounter) {
    int maxCoverage = endCounter - g_startCounter;
    int coveredAsserts = 0;
    for (int i=0;i<maxCoverage;i++) {
        if (g_coverageVector[i/8] & (1 << (i%8))) {
            coveredAsserts++;
        }
    }
    if (coveredAsserts == maxCoverage && g_totalAsserts == g_passedAsserts) {
        _trial_printf("[TRIAL] SUCCESS 100%% (asserts %d/%d, coverage %d/%d)\n",
            g_passedAsserts, g_totalAsserts, coveredAsserts, maxCoverage);
    } else {
        _trial_printf("[TRIAL] FAILED %d%% (asserts %d/%d, coverage %d/%d)\n",
            100 * (g_passedAsserts + coveredAsserts) / (g_totalAsserts + maxCoverage),
            g_passedAsserts, g_totalAsserts, coveredAsserts, maxCoverage);
    }
    _trial_printf("[TRIAL] End\n");

    SYS_system_operation(ELOS_SYSOP_SHUTDOWN, NULL, 0);
}


#endif // TRIAL_IMPL

