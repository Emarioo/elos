#pragma once


typedef struct {

    int needsReboot;
    // targets to test
    // files

} Template;


#define MAX_ASSERTS 1024

extern char g_coverageVector[MAX_ASSERTS/8];
int g_startCounter;

void _trial_printf(const char* format, ...);

void _trial_assert(const char* name, int cond);
void _trial_start(int startCounter);
void _trial_end(int endCounter);


#define trial_start() _trial_start(__COUNTER__)
#define trial_end() _trial_end(__COUNTER__)

#define trial_assert(NAME, COND) do {                  \
    int tv_index = __COUNTER__ - g_startCounter;       \
    _trial_assert(NAME, COND);                         \
    g_coverageVector[tv_index/8] = 1 << (tv_index%8);  \
    } while (0)

#ifdef TRIAL_IMPL

int g_totalAsserts;
int g_passedAsserts;
char g_coverageVector[MAX_ASSERTS/8];


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
        _trial_printf("PASS %s\n", name);
        g_passedAsserts++;
    } else {
        _trial_printf("FAIL %d\n", name);
    }
    g_totalAsserts++;
}
void _trial_start(int startCounter) {
    _trial_printf("Start trial\n");
    g_startCounter = startCounter;
    g_totalAsserts = 0;
    g_passedAsserts = 0;
    memset(g_coverageVector, 0, sizeof(g_coverageVector));
}
void _trial_end(int endCounter) {
    int maxCoverage = endCounter - g_startCounter;
    int coveredAsserts = 0;
    for (int i=0;i<maxCoverage;i++) {
        if (g_coverageVector[i/8] & (1 << (i%8)))
            coveredAsserts++;
    }
    if (coveredAsserts == maxCoverage && g_totalAsserts == g_passedAsserts) {
        _trial_printf("SUCCESS 100% (%d/%d, coverage %d/%d)\n",
            g_passedAsserts, g_totalAsserts, coveredAsserts, maxCoverage);
    } else {
        _trial_printf("FAILED %d%% (%d/%d, coverage %d/%d)\n",
            100 * (g_passedAsserts + coveredAsserts) / (g_totalAsserts + maxCoverage),
            g_passedAsserts, g_totalAsserts, coveredAsserts, maxCoverage);
    }
}


#endif // TRIAL_IMPL

