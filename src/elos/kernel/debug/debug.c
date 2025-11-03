
#include "elos/kernel/debug/debug.h"
#include "elos/kernel/common/intrinsics.h"
#include "elos/kernel/common/string.h"

static bool _serial_initialized;

void serial_write(const cstring text) {
    const u16 COM1 = 0x3F8;
    if(!_serial_initialized) {
        _serial_initialized = true;
        outb(COM1 + 1, 0x00); // Disable interrupts
        outb(COM1 + 3, 0x80); // Enable DLAB
        outb(COM1 + 0, 0x01); // 38400 baud divisor (lo byte)
        outb(COM1 + 1, 0x00); // (hi byte)
        outb(COM1 + 3, 0b110); // 8 bits, no parity, one stop
        outb(COM1 + 2, 0xC7); // Enable FIFO, clear them, 14-byte threshold
    }

    for (int i = 0; i < text.len; i++) {
        while (1) {
            // @TODO: After some number of attempts, stop, serial probably isn't available.
            u8 status = inb(COM1 + 5);
            if ((status & 0x20) != 0)
                break;
        }
        outb(COM1, text.ptr[i] & 0x7F);
    }
}

void serial_printf(const char* format, ...) {
    char buffer[256];
    unsigned short w_buffer[256];

    va_list va;
    va_start(va, format);
    const int len = vsnprintf(buffer, sizeof(buffer), format, va);
    va_end(va);

    cstring text = { buffer, len };
    serial_write(text);
}
