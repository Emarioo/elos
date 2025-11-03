#include "elos/kernel/common/string.h"
#include "elos/kernel/common/types.h"

#include <stdarg.h>

static int output_int(char* buffer, int size, int value) {
    if (!buffer || !size)
        return 0;

    int head = 0;
    int acc = 0;

    #define CHECK if (head-1 >= size) { buffer[head] = '\0'; return head; }

    CHECK

    if (value < 0) {
        buffer[head] = '-';
        head++;
        acc = -value;
    } else {
        acc = value;
    }
    int digits = 0;
    do {
        // (accst % 10) + '0';
        acc = acc / 10;
        digits++;
    } while (acc);
    
    if (value < 0) {
        acc = -value;
    } else {
        acc = value;
    }
    
    do {
        buffer[head+digits-1] = (acc % 10) + '0';
        digits-=2;
        head++;
        CHECK
        acc = acc / 10;
    } while (acc);

    buffer[head] = '\0';
    return head;
    #undef CHECK
}

static int output_hex(char* buffer, int size, u32 value, int width) {
    if (!buffer || !size)
        return 0;

    int head = 0;
    u32 acc = value;

    #define CHECK if (head-1 >= size) { buffer[head] = '\0'; return head; }

    CHECK

    int digits = 0;
    do {
        acc = acc / 16;
        digits++;
    } while (acc);
    
    for (int i = 0; i < width - digits; i++) {
        buffer[head] = '0';
        head++;
        CHECK
    }

    acc = value;
    
    do {
        u32 val = (acc % 16);
        buffer[head+digits-1] = val < 10 ? val + '0' : val - 10 + 'a';
        digits-=2;
        head++;
        CHECK
        acc = acc / 16;
    } while (acc);

    buffer[head] = '\0';
    return head;
    #undef CHECK
}

int vsnprintf(char* buffer, int size, const char* format, va_list va) {
    if(!buffer || !size)
        return 0;

    int format_len = strlen(format);
    int head = 0;
    int i = 0;
    
    #define CHECK if (head-1 >= size) { buffer[head] = '\0'; return head; }
    
    while (i < format_len) {
        if (format[i] != '%') {
            buffer[head] = format[i];
            head++;
            CHECK

            i++;
            continue;
        }
        i++;
        if (i >= format_len)
            break;        

        int width = 0;

        if (format[i] >= '0' && format[i] <= '9') {
            width = format[i] - '0';
            i++;
        }

        if (i >= format_len)
            break;

        if (format[i] == 'd') {
            i++;

            int value = va_arg(va, int);
            int len = output_int(buffer + head, size - head, value);
            head += len;
            CHECK
        } else if (format[i] == 'c') {
            i++;

            char value = va_arg(va, int);
            buffer[head] = value;
            head += 1;
            CHECK
        } else if (format[i] == 'x') {
            i++;

            int value = va_arg(va, int);

            // if (width > 0) {
            //     int num_leading_zero_bits;

            //     // asm("lzcnt %1, %0"
            //     //     : "=r"(num_leading_zero_bits)
            //     //     : "r"(value));
                
            //     int padding = width > (32-num_leading_zero_bits) / 4 ? width - (32-num_leading_zero_bits) / 4 : 0;

            //     for (int i = 0; i < padding; i++) {
            //         buffer[head] = '0';
            //         head++;
            //         CHECK
            //     }
            // }

            int len = output_hex(buffer + head, size - head, value, width);
            head += len;
            CHECK
        } else if (format[i] == 's') {
            i++;
            
            const char* value = va_arg(va, const char*);
            int len = strlen(value);
            
            len = len > size-head ? size-head : len;
            memcpy(buffer + head, value, len);
            head += len;
            CHECK
        } else {
            buffer[head] = '%';
            head++;
            CHECK
        }
    }

    buffer[head] = '\0';
    return head;
}

int snprintf(char* buffer, int size, const char* format, ...) {
    va_list va;
    va_start(va, format);
    const int res = vsnprintf(buffer, size, format, va);
    va_end(va);
    return res;
}
