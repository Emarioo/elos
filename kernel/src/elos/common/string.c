
#include "elos/common/string.h"


// void* __memset_chk() { }

static int output_int(char* buffer, size_t size, size_t value, bool isSigned, int width, int zeroPadding) {
    if (!buffer || !size)
        return 0;

    int head = 0;
    size_t acc = 0;

    #define CHECK if (head > size) { buffer[head] = '\0'; return head; }

    CHECK

    if (isSigned && (int64_t)value < 0) {
        buffer[head] = '-';
        head++;
        acc = -(int64_t)value;
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

    for (int i=0;i<zeroPadding - digits;i++) {
        buffer[head] = '0';
        head++;
        CHECK
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

static int output_hex(char* buffer, size_t size, size_t value, int width) {
    if (!buffer || !size)
        return 0;

    int head = 0;
    size_t acc = value;

    #define CHECK if (head > size) { buffer[head] = '\0'; return head; }

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

int vsnprintf(char* buffer, size_t _size, const char* format, va_list va) {
    if(!buffer || !_size)
        return 0;

    int size = (int)_size; // Bad, assert warning if size is large
    int format_len = strlen(format);
    int head = 0;
    int i = 0;
    
    #define CHECK if (head > size) { buffer[head] = '\0'; return head; }
    
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
        int hasStringLength = 0;
        int stringLength = 0;
        int zeroPadding = 0;

        if (format[i] == '*') {
            width = va_arg(va, int);
            i++;
        } else if (format[i] >= '0' && format[i] <= '9') {
            width = format[i] - '0';
            i++;
        }
        if (i+1 < format_len && format[i] == '.' && format[i+1] >= '0' && format[i+1] <= '9') {
            zeroPadding = format[i+1] - '0';
            i += 2;
        }
        if (i+1 < format_len && format[i] == '.' && format[i+1] == '*') {
            hasStringLength = true;
            i += 2;
            stringLength = va_arg(va, int);
        }

        if (i >= format_len)
            break;

        if (i+1 < format_len && format[i] == 'z' && (format[i+1] == 'd' || format[i+1] == 'i')) {
            i+=2;
            size_t value = va_arg(va, size_t);
            int len = output_int(buffer + head, size - head, value, true, 0, zeroPadding);
            head += len;
            CHECK
        } else if (i+1 < format_len && format[i] == 'z' && (format[i+1] == 'u')) {
            i+=2;
            size_t value = va_arg(va, size_t);
            int len = output_int(buffer + head, size - head, value, false, 0, zeroPadding);
            head += len;
            CHECK
        } else if (format[i] == 'd' || format[i] == 'i') {
            i++;
            int value = va_arg(va, int);
            int len = output_int(buffer + head, size - head, value, true, 0, zeroPadding);
            head += len;
            CHECK
        } else if (format[i] == 'u') {
            i++;
            u32 value = va_arg(va, u32);
            int len = output_int(buffer + head, size - head, value, false, 0, zeroPadding);
            head += len;
            CHECK
        } else if (format[i] == 'c') {
            i++;

            char value = va_arg(va, int);
            if (value == '\0') {
                buffer[head] = '\\';
                buffer[head+1] = '0';
                head += 2;
            } else if (value == '\n') {
                buffer[head] = '\\';
                buffer[head+1] = 'n';
                head += 2;
            } else if (value == '\t') {
                buffer[head] = '\\';
                buffer[head+1] = 't';
                head += 2;
            } else {
                buffer[head] = value;
                head += 1;
            }
            CHECK

        } else if (i+1 < format_len && format[i] == 'z' && (format[i+1] == 'x')) {
            i+=2;
            size_t value = va_arg(va, size_t);
            int len = output_hex(buffer + head, size - head, value, width);
            head += len;
            CHECK
        } else if (format[i] == 'p') {
            i+=1;
            size_t value = va_arg(va, size_t);
            int len = output_hex(buffer + head, size - head, value, width);
            head += len;
            CHECK
        } else if (format[i] == 'x') {
            i++;

            uint32_t value = va_arg(va, uint32_t);

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
            int len = hasStringLength ? stringLength : strlen(value);
            
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

int snprintf(char* buffer, size_t size, const char* format, ...) {
    va_list va;
    va_start(va, format);
    const int res = vsnprintf(buffer, size, format, va);
    va_end(va);
    return res;
}




long strtol(const char* ptr, char** endptr, int base) {

    #define isdigit(C) ( (C) >= '0' && (C) <= '9' )

    int head = 0;
    long acc = 0;
    int isNegative = 0;
    int len = strlen(ptr);
    if (base != 10 && base != 8 && base != 16)
        base = 10;

    if (ptr[head] == '-') {
        isNegative = 1;
        head++;
    }

    while (head < len) {
        char c = ptr[head];
        if (base == 8 && c >= '0' && c <= '7') {
            acc = acc * base + c - '0';
            head++;
            continue;
        } else if (base == 10 && c >= '0' && c <= '9') {
            acc = acc * base + c - '0';
            head++;
            continue;
        } else if (base == 16) {
            if (c >= '0' && c <= '9') {
                acc = acc * base + c - '0';
                head++;
                continue;
            } else if ((c|32) >= 'a' && (c|32) <= 'f') {
                acc = acc * base + c - 'a';
                head++;
                continue;
            }
        }
        break;
    }
    if (endptr) {
        *endptr = (char*)ptr + head;
    }
    if (isNegative) {
        acc = -acc;
    }

    return acc;
}




size_t strlen(const char* ptr) {
    const char* base = ptr;
    while(*(ptr++)) ;
    return (u64)ptr - (u64)base - 1;
}
size_t strnlen(const char* ptr, size_t maxlen) {
    int index = 0;
    while(1) {
        if (index >= maxlen)
            return maxlen;
        char chr = ptr[index];
        if (chr == '\0')
            break;
        index++;
    }
    return index;
}

// void memcpy(void* dst, const void* src, int size) {
//     if (dst == src)
//         return;
//     for (int i=0;i<size;i++) {
//         ((char*)dst)[i] = ((char*)src)[i];
//     }
// }

void* memmove(void* dst, const void* src, size_t size) {
    if (dst == src)
        return dst;
    
    if ((u64)dst % 8 == 0 && (u64)src % 8 == 0 && size % 8 == 0) {
        // aligned
        if (dst < src) {
            for (int i=0;i<(int)size/8;i++) {
                ((u64*)dst)[i] = ((u64*)src)[i];
            }
        } else {
            for (int i=(int)size/8-1;i>=0;i--) {
                ((u64*)dst)[i] = ((u64*)src)[i];
            }
        }
    } else {
        if (dst < src) {
            for (int i=0;i<(int)size;i++) {
                ((char*)dst)[i] = ((char*)src)[i];
            }
        } else {
            for (int i=(int)size-1;i>=0;i--) {
                ((char*)dst)[i] = ((char*)src)[i];
            }
        }
    }
    return dst;
}

int memcmp(const void* dst, const void* src, size_t size) {
    for(int i=0;i<size;i++) {
        if ( ((char*)dst)[i] != ((char*)src)[i] )
            // Did i flip the subtraction?
            return ((char*)dst)[i] - ((char*)src)[i];
    }
    return 0;
}

int strcmp(const char* dst, const char* src) {
    int i=0;
    while (1) {
        char s = src[i];
        char d = dst[i];
        if (d != s) {
            return d - s;
        }
        if (d == 0) {
            break;
        }
        i++;
    }
    return 0;
}

int strncmp(const char* dst, const char* src, size_t len) {
    int i=0;
    while (i < len) {
        char s = src[i];
        char d = dst[i];
        if (d != s) {
            return d - s;
        }
        if (d == 0) {
            break;
        }
        i++;
    }
    return 0;
}

void* memset(void* dst, int val, size_t size) {
    for(size_t i = 0; i < size; i++) {
        *((char*)dst + i) = val;
    }
    return dst;
}


u16* tmp_path_wstring(const char* str) {
    static u16 wstr[256];
    int len = strlen(str);
    for (int i = 0; i < len && i < 256-1; i++) {
        wstr[i] = str[i];
    }
    wstr[len] = 0;
    return wstr;
}


char *strstr(const char *haystack, const char *needle)
{
    if (*needle == 0)
        return (char *)haystack;

    while (*haystack)
    {
        const char *h = haystack;
        const char *n = needle;

        while (*h && *n && *h == *n)
        {
            h++;
            n++;
        }

        if (*n == 0)
            return (char *)haystack;

        haystack++;
    }

    return NULL;
}

