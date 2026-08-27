
#include "elos/kernel_console.h"

#include "elos/common/string.h"
#include "elos/common/intrinsics.h"
#include "elos/common/types.h"

#include "elos/network.h"
#include "elos/cpu.h"

#include "elos/kernel/kcon/netlog.h"
// #include "elos/kernel/net/net_internal.h"


// @TODO Implement serial device?
// #include "elos/serial_device.h"


void serial_init();



FN_KCON_write _write_hooks[4];

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
    char buffer[512];

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
    static u32 print_lock;

    LOCK_INT(&print_lock);

    for (int i = 0; i < size; i++) {
        int limit = 100;
        while (limit--) {
            u8 status = inb(COM1 + 5);
            if ((status & 0x20) != 0)
                break;
        }
        outb(COM1, buffer[i] & 0x7F);
    }

    UNLOCK_INT(&print_lock);
}


u8        netlog_target_mac[6];
u32       netlog_target_address;
NetDevice netlog_device;

void KCON_net_set_target(NetDevice device, u8 mac[6], u32 address) {
    netlog_device = device;
    memcpy(netlog_target_mac, mac, 6);
    netlog_target_address = address;
}

void KCON_net_write(const char* buffer, int buffer_len) {
    static int sending;
    static int sequence;

    if (sending)
        return;

    if (!netlog_target_address) {
        KCON_printf("NETLOG Target address is not set\n");
        return;
    }
    
    sending++;

    u8 chunk[1600];

    int head = 0;
    while (head < buffer_len) {
        NetLog_Header* header = (NetLog_Header*)chunk;
        header->command = NETLOG_COMMAND_DATA;
        memcpy(header->magic, NETLOG_MAGIC, sizeof(header->magic));
        header->sequence = sequence++;
        if (buffer_len - head + sizeof(NetLog_Header) > 1400) {
            header->size = 1400;
        }
        memcpy(header->payload, buffer + head, header->size);
        head += header->size;
        bool sent = NET_send_udp(netlog_device, netlog_target_mac, netlog_target_address, NETLOG_DEFAULT_PORT, NETLOG_DEFAULT_PORT, chunk, header->size + sizeof(NetLog_Header));
        
        // Do nothing if we failed sending?
    }

    sending--;
    
}


void kernel_panic(const char* format, ...) {
    char buffer[512];

    va_list va;
    va_start(va, format);
    int len = vsnprintf(buffer, sizeof(buffer), format, va);
    va_end(va);

    for (int i=0;i<ARRAY_LENGTH(_write_hooks);i++) {
        if (_write_hooks[i]) {
            _write_hooks[i](buffer, len);
        }
    }

    // @TODO Tell other cores to halt too.
    while (1) {
        asm volatile (
            "cli\n"
            "hlt\n"
        );
    }
}
