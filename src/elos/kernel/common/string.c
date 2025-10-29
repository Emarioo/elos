#include "elos/kernel/common/string.h"
#include "elos/kernel/common/types.h"

#include "stdarg.h"

static int parse_int(char* buffer, int size, int value) {
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

int vsnprintf(char* buffer, int size, char* format, va_list va) {
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

        if (format[i] == 'd') {
            i++;

            int value = va_arg(va, int);
            int len = parse_int(buffer + head, size - head, value);
            head += len;
            CHECK
        } else if (format[i] == 'c') {
            i++;

            char value = va_arg(va, int);
            buffer[head] = value;
            head += 1;
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

int snprintf(char* buffer, int size, char* format, ...) {
    va_list va;
    va_start(va, format);
    const int res = vsnprintf(buffer, size, format, va);
    va_end(va);
    return res;
}
