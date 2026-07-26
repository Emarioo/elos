
#include "elos/common/string.h"

#include "elos/syscalls.h"

#include <stdarg.h>
#include "async_io.h"

#include "stdlib.h"
#include "stdio.h"
#include <errno.h>

typedef u32 mode_t;


struct FILE {
    ELOS_File file;
    uintptr_t position;
};

int printf(const char* format, ...) {
    char buffer[400];

    va_list va;
    va_start(va, format);
    int len = vsnprintf(buffer, sizeof(buffer), format, va);
    va_end(va);

    SYS_debug_log(buffer, len);
    return len;
}


FILE* stdin  = (FILE*)0;
FILE* stderr = (FILE*)1;
FILE* stdout = (FILE*)2;


int fprintf(FILE* stream, const char* format, ...) {
    char buffer[400];

    va_list va;
    va_start(va, format);
    int len = vsnprintf(buffer, sizeof(buffer), format, va);
    va_end(va);

    SYS_debug_log(buffer, len);
    return len;
}

int vfprintf(FILE* stream, const char* format, va_list args) {
    char buffer[400];

    int len = vsnprintf(buffer, sizeof(buffer), format, args);

    SYS_debug_log(buffer, len);
    return len;
}

int fflush(FILE* stream) {
    (void)stream;
    return 0;
}

FILE *fopen(const char *restrict path, const char *restrict mode) {
    ELOS_Error error;

    ELOS_AsyncRequest req;
    ELOS_AsyncCompletion cqe;
    Async_RequestID requestID;

    req.operation   = ELOS_ASYNC_FILE_OPEN;
    req.flags       = 0;

    req.open.path   = path;
    if (mode[0] == 'r') {
        if (mode[1] == '+') {
            req.open.flags  = ELOS_FILE_OPEN_FLAG_READ_ONLY;
        } else {
            req.open.flags  = 0; // Reading and writing, no truncation
        }
    } else {
        // @TODO What if we want to open file for writing no truncation?
        req.open.flags  = ELOS_FILE_OPEN_FLAG_CREATE;
    }

    requestID = async_submit(&req);
    bool res = async_wait(requestID, &cqe, 0);
    if (!res) {
        return NULL;
    }

    if (cqe.error != ELOS_OK) {
        return NULL;
    }

    FILE* file = malloc(sizeof(FILE));
    if (!file) {
        return NULL;
    }
    file->file = cqe.open.file;
    file->position = 0;

    return file;
}
int fclose(FILE *file) {
    ELOS_Error error;

    ELOS_AsyncRequest req;
    ELOS_AsyncCompletion cqe;
    Async_RequestID requestID;

    req.operation   = ELOS_ASYNC_FILE_CLOSE;
    req.flags       = 0;

    req.close.file  = file->file;

    requestID = async_submit(&req);
    bool res = async_wait(requestID, &cqe, 0);
    if (!res) {
        return -1;
    }

    if (cqe.error != ELOS_OK) {
        return -1;
    }

    free(file);

    return 0;
}

int fseek(FILE *stream, long offset, int whence) {
    if (whence == SEEK_SET) {
        stream->position = offset;
        return 0;
    } else if (whence == SEEK_CUR) {
        stream->position += offset;
        return 0;
    } else if (whence != SEEK_END) {
        return -1;
    }

    ELOS_Error error;

    ELOS_AsyncRequest req;
    ELOS_AsyncCompletion cqe;
    Async_RequestID requestID;
    ELOS_FileInfo fileInfo;

    req.operation   = ELOS_ASYNC_FILE_INFO;
    req.flags       = 0;

    req.info.file   = stream->file;
    req.info.fileInfo = &fileInfo;

    requestID = async_submit(&req);
    bool res = async_wait(requestID, &cqe, 0);
    if (!res) {
        return -1;
    }

    if (cqe.error != ELOS_OK) {
        return -1;
    }

    if (whence != SEEK_END) {
        // Shouldn't happen.
        return -1;
    }
        
    stream->position = fileInfo.fileSize + offset;

    return 0;
}
long ftell(FILE *stream) {
    return stream->position;
}

size_t fread(void* ptr, size_t size, size_t n, FILE *restrict stream) {
    ELOS_Error error;

    ELOS_AsyncRequest req;
    ELOS_AsyncCompletion cqe;
    Async_RequestID requestID;

    req.operation   = ELOS_ASYNC_FILE_READ;
    req.flags       = 0;

    req.read.file   = stream->file;
    req.read.buffer = ptr;
    req.read.offset = stream->position;
    req.read.size   = size * n;

    requestID = async_submit(&req);
    bool res = async_wait(requestID, &cqe, 0);
    if (!res) {
        return 0;
    }

    if (cqe.error != ELOS_OK) {
        return 0;
    }

    stream->position += cqe.read.readBytes;

    return cqe.read.readBytes;
}
size_t fwrite(const void* ptr, size_t size, size_t n, FILE *restrict stream) {
    if (stream == stderr || stream == stdout) {
        SYS_debug_log(ptr, size * n);
        return n;
    }

    ELOS_Error error;

    ELOS_AsyncRequest req;
    ELOS_AsyncCompletion cqe;
    Async_RequestID requestID;

    req.operation   = ELOS_ASYNC_FILE_WRITE;
    req.flags       = 0;

    req.write.file   = stream->file;
    req.write.buffer = ptr;
    req.write.offset = stream->position;
    req.write.size   = size * n;

    requestID = async_submit(&req);
    bool res = async_wait(requestID, &cqe, 0);
    if (!res) {
        return 0;
    }

    if (cqe.error != ELOS_OK) {
        return 0;
    }

    stream->position += cqe.write.writtenBytes;

    return cqe.write.writtenBytes;
}


static int is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\n' ||
           c == '\r' || c == '\f' || c == '\v';
}

static int digit_value(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static bool parse_int(const char **str, int base, bool allow_prefix, int *out)
{
    const char *p = *str;

    while (is_space(*p))
        p++;

    int sign = 1;
    if (*p == '-') {
        sign = -1;
        p++;
    } else if (*p == '+') {
        p++;
    }

    if (allow_prefix) {
        if (base == 0) {
            if (p[0] == '0') {
                if (p[1] == 'x' || p[1] == 'X') {
                    base = 16;
                    p += 2;
                } else {
                    base = 8;
                    p++;
                }
            } else {
                base = 10;
            }
        } else if (base == 16) {
            if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X'))
                p += 2;
        }
    }

    unsigned value = 0;
    bool found = false;

    while (1) {
        int d = digit_value(*p);
        if (d < 0 || d >= base)
            break;

        value = value * base + d;
        found = true;
        p++;
    }

    if (!found)
        return false;

    *out = (int)(value * sign);
    *str = p;
    return true;
}

int vsscanf(const char *str, const char *fmt, va_list args)
{
    int assigned = 0;

    while (*fmt) {

        if (is_space(*fmt)) {
            while (is_space(*fmt))
                fmt++;

            while (is_space(*str))
                str++;

            continue;
        }

        if (*fmt != '%') {
            if (*fmt != *str)
                break;

            fmt++;
            str++;
            continue;
        }

        fmt++;

        int base;
        bool allow_prefix = false;

        switch (*fmt) {
        case 'd':
            base = 10;
            break;

        case 'o':
            base = 8;
            break;

        case 'x':
            base = 16;
            allow_prefix = true;
            break;

        case 'i':
            base = 0;
            allow_prefix = true;
            break;

        case '%':
            if (*str != '%')
                return assigned;
            str++;
            fmt++;
            continue;

        default:
            return assigned;
        }

        int *out = va_arg(args, int *);

        if (!parse_int(&str, base, allow_prefix, out))
            return assigned;

        assigned++;
        fmt++;
    }

    return assigned;
}

int sscanf(const char *str, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int ret = vsscanf(str, fmt, args);
    va_end(args);
    return ret;
}


int remove(const char* path) {
    ELOS_Error error;

    ELOS_AsyncRequest req;
    ELOS_AsyncCompletion cqe;
    Async_RequestID requestID;

    req.operation   = ELOS_ASYNC_FILE_REMOVE;
    req.flags       = 0;

    req.remove.path  = path;

    requestID = async_submit(&req);
    bool res = async_wait(requestID, &cqe, 0);
    if (!res) {
        errno = ENOENT;
        return -1;
    }

    if (cqe.error != ELOS_OK) {
        errno = ENOENT;
        return -1;
    }

    return 0;
}

int rename(const char* oldpath, const char* newpath) {
    ELOS_Error error;

    ELOS_AsyncRequest req;
    ELOS_AsyncCompletion cqe;
    Async_RequestID requestID;

    req.operation   = ELOS_ASYNC_FILE_RENAME;
    req.flags       = 0;

    req.rename.oldPath = oldpath;
    req.rename.newPath = newpath;

    requestID = async_submit(&req);
    bool res = async_wait(requestID, &cqe, 0);
    if (!res) {
        errno = ENOENT;
        return -1;
    }

    if (cqe.error != ELOS_OK) {
        errno = ENOENT;
        return -1;
    }

    return 0;
}
int mkdir(const char* path, mode_t mode) {
    ELOS_Error error;

    ELOS_AsyncRequest req;
    ELOS_AsyncCompletion cqe;
    Async_RequestID requestID;

    req.operation   = ELOS_ASYNC_FILE_MKDIR;
    req.flags       = 0;

    req.mkdir.path = path;

    requestID = async_submit(&req);
    bool res = async_wait(requestID, &cqe, 0);
    if (!res) {
        errno = ENOENT;
        return -1;
    }

    if (cqe.error != ELOS_OK) {
        errno = ENOENT;
        return -1;
    }

    return 0;
}


ELOS_Error elos_readdir(const char* path, u64* cookie, u64* entryCount, ELOS_DirectoryEntry* buffer) {
    
    ELOS_Error error;

    ELOS_AsyncRequest req;
    ELOS_AsyncCompletion cqe;
    Async_RequestID requestID;

    req.operation   = ELOS_ASYNC_FILE_READDIR;
    req.flags       = 0;

    req.readdir.path = path;
    req.readdir.cookie = *cookie;
    req.readdir.maxEntries = *entryCount;
    req.readdir.buffer = buffer;

    requestID = async_submit(&req);
    bool res = async_wait(requestID, &cqe, 0);
    if (!res) {
        return ELOS_GENERIC_ERROR;
    }

    *cookie = cqe.readdir.cookie;
    *entryCount = cqe.readdir.entryCount;

    return cqe.error;
}



