
#include "elos/trace.h"

#include "elos/common/string.h"
#include "elos/cpu.h"
#include "elos/kernel_console.h"

bool g_traceEnabled[TRACE_COUNT];

u64 g_trace_tick_offset;


extern void serial_write(const char* buffer, int size);


void init_trace() {
    g_trace_tick_offset = CPU_ticks();

    // memset(&g_traceEnabled, 0x1, TRACE_COUNT);
}

void tracef(Trace_Category category, const char* format, ...) {
    char buffer[512];
    int len = 0;

    if (!g_traceEnabled[category]) {
        // KCON_printf("Not active\n");
        return;
    }

    u64 ticks_per_second = CPU_ticks_per_second();

    u64 now = CPU_ticks();
    u64 delta = now - g_trace_tick_offset;

    u64 hours   = (((delta/ticks_per_second)/60)/60) % 60;
    u64 minutes = ((delta/ticks_per_second)/60) % 60;
    u64 seconds = (delta/ticks_per_second) % 60;
    u64 milliseconds = (1000 * delta / ticks_per_second) % 1000;
    u64 microseconds = ((1000 * 1000 * delta) / ticks_per_second) % 1000;

    // 25:162:157.751.552
    len += snprintf(buffer + len, sizeof(buffer) - len, "[%02u:%02u:%02u.%03u.%03u] ",
        (u32)hours, (u32)minutes, (u32)seconds, (u32)milliseconds, (u32)microseconds);

    va_list va;
    va_start(va, format);
    len += vsnprintf(buffer + len, sizeof(buffer) - len, format, va);
    va_end(va);

    serial_write(buffer, len);
}

