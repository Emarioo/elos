
#include "elos/kernel_console.h"

#include "elos/kernel/common/string.h"

#include <stdint.h>
#include <stdbool.h>

// @TODO Implement serial device?
// #include "elos/serial_device.h"

#define ARRAY_LENGTH(ARR) sizeof(ARR)/sizeof(*ARR)

void serial_init();
void serial_write(const char* buffer, int size);



static FN_KCON_write _write_hooks[4];

void KCON_init(BootAPI* boot_api) {
    serial_init();

    KCON_add_write_hook(serial_write);
}

void KCON_add_write_hook(FN_KCON_write func) {
    for (int i=0;i<ARRAY_LENGTH(_write_hooks);i++) {
        if (_write_hooks[i] == NULL) {
            _write_hooks[i] = func;
            break;
        }
    }
}

void KCON_printf(const char* format, ...) {
    char buffer[256];

    va_list va;
    va_start(va, format);
    int len = vsnprintf(buffer, sizeof(buffer), format, va);
    va_end(va);

    for (int i=0;i<ARRAY_LENGTH(_write_hooks);i++) {
        if (_write_hooks[i]) {
            _write_hooks[i](buffer, len);
        }
    }
}




#define COM1 0x3F8


void serial_init() {
    outb(COM1 + 1, 0x00); // Disable interrupts
    outb(COM1 + 3, 0x80); // Enable DLAB
    outb(COM1 + 0, 0x01); // 38400 baud divisor (lo byte)
    outb(COM1 + 1, 0x00); // (hi byte)
    outb(COM1 + 3, 0b110); // 8 bits, no parity, one stop
    outb(COM1 + 2, 0xC7); // Enable FIFO, clear them, 14-byte threshold
}



void serial_write(const char* buffer, int size) {
    for (int i = 0; i < size; i++) {
        int limit = 100;
        while (limit--) {
            uint8_t status = inb(COM1 + 5);
            if ((status & 0x20) != 0)
                break;
        }
        outb(COM1, buffer[i] & 0x7F);
    }
}
