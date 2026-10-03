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

typedef struct {
    const char* filePath;
    uint32_t    line;
    uint32_t    executed;
} TrialAssert;

extern TrialAssert  __trial_asserts_start[];
extern TrialAssert  __trial_asserts_end[];

void _trial_printf(const char* format, ...);

void _trial_start();
void _trial_end();

void _trial_assert(const char* name, int cond);
void _trial_assert_int(const char* name, int valueA, int valueB);


#define trial_start() _trial_start()
#define trial_end() _trial_end()

#define _TRIAL_BASE_ASSERT(ASSERT_FUNC, NAME, ...) do {                \
    ASSERT_FUNC (NAME, __VA_ARGS__);                                   \
    __attribute__((section(".trial_asserts")))                         \
    static TrialAssert info = {                                         \
        __FILE__, __LINE__, 0                                          \
    };                                                                 \
    info.executed++;                                                   \
    } while (0)

#define trial_assert(NAME, ...)     _TRIAL_BASE_ASSERT(_trial_assert, NAME, __VA_ARGS__)
#define trial_assert_int(NAME, ...) _TRIAL_BASE_ASSERT(_trial_assert_int, NAME, __VA_ARGS__)

#ifdef TRIAL_IMPL


uint32_t g_totalAsserts;
uint32_t g_passedAsserts;


void _trial_assert(const char* name, int cond) {
    if (cond) {
        _trial_printf("[TRIAL] PASS %s, %d\n", name, cond);
        g_passedAsserts++;
    } else {
        _trial_printf("[TRIAL] FAIL %s, %d\n", name, cond);
    }
    g_totalAsserts++;
}

void _trial_assert_int(const char* name, int valueA, int valueB) {
    if (valueA == valueB) {
        _trial_printf("[TRIAL] PASS %s, %d\n", name, valueA);
        g_passedAsserts++;
    } else {
        _trial_printf("[TRIAL] FAIL %s, %d == %d\n", name, valueA, valueB);
    }
    g_totalAsserts++;
}



void _trial_printf(const char* format, ...) {
    char buffer[256];

    va_list va;
    va_start(va, format);
    int len = vsnprintf(buffer, sizeof(buffer), format, va);
    va_end(va);
    
    SYS_debug_log(buffer, len);
}

void _trial_start() {
    _trial_printf("[TRIAL] Start\n");
    g_totalAsserts = 0;
    g_passedAsserts = 0;
}
void _trial_end() {
    uint32_t maxCoverage = &__trial_asserts_end[0] - &__trial_asserts_start[0];
    uint32_t coveredAsserts = 0;
    for (uint32_t i = 0; i < maxCoverage; i++) {
        TrialAssert* info = &__trial_asserts_start[i];
        if (info->executed > 0) {
            coveredAsserts++;
        } else {
            printf("[TRIAL] Assert Missed: %s:%u\n", info->filePath, info->line);
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

