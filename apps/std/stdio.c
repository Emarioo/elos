
#include "elos/common/string.h"


#include <stdarg.h>
#include "async_io.h"

#include "stdlib.h"
#include "stdio.h"
#include <errno.h>



struct FILE {
    ELOS_File file;
    uintptr_t position;

    void*     cache;
    size_t    cache_max;
    size_t    cache_len;
    uintptr_t cache_pos;

    void*     rcache;
    size_t    rcache_max;
    size_t    rcache_len;
    size_t    rcache_readOffset;
    uintptr_t rcache_pos;
};

FILE* stdin  = (FILE*)0;
FILE* stderr = (FILE*)1;
FILE* stdout = (FILE*)2;

char cwd[256];
int  cwd_len;


static bool flush_write_cache(FILE *restrict stream);


#define GET_ABS_PATH(outPATH, PATH) \
    char temp##outPATH[256];    \
    resolveAbsPath(temp##outPATH, sizeof(temp##outPATH), PATH); \
    char* outPATH = temp##outPATH;


int printf(const char* format, ...) {
    char buffer[400];

    va_list va;
    va_start(va, format);
    int len = vsnprintf(buffer, sizeof(buffer), format, va);
    va_end(va);

    SYS_debug_log(buffer, len);
    return len;
}

int resolveAbsPath(char* buffer, int bufferMax, const char* path) {
    if (path[0] == '/') {
        return snprintf(buffer, bufferMax, "%s", path);
    } else if (cwd[cwd_len-1] == '/') {
        return snprintf(buffer, bufferMax, "%s%s", cwd ,path);
    } else {
        return snprintf(buffer, bufferMax, "%s/%s", cwd, path);
    }
}


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

FILE *fopen(const char *restrict _path, const char *restrict mode) {
    ELOS_Error error;

    GET_ABS_PATH(path, _path);

    ELOS_AsyncRequest req;
    ELOS_AsyncCompletion cqe;
    Async_RequestID requestID;

    req.operation   = ELOS_ASYNC_FILE_OPEN;
    req.flags       = 0;

    req.open.path   = path;
    if (mode[0] == 'r') {
        if (mode[1] == '+') {
            req.open.flags  = 0; // Reading and writing, no truncation
        } else {
            req.open.flags  = ELOS_FILE_OPEN_FLAG_READ_ONLY;
        }
    } else if(mode[0] == 'w') {
        req.open.flags  = ELOS_FILE_OPEN_FLAG_CREATE;
    } else {
        // Mode not supported. Append 'a' is one of them.
        return NULL;
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
    memset(file, 0, sizeof(*file));
    file->file = cqe.open.file;
    file->position = 0;

    return file;
}
int fclose(FILE *file) {

    int has_flush_error = !flush_write_cache(file);

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

    return has_flush_error;
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


static bool flush_read_cache(FILE *restrict stream) {
    if (!stream->cache_len) {
        return true;
    }

    ELOS_Error error;

    ELOS_AsyncRequest req;
    ELOS_AsyncCompletion cqe;
    Async_RequestID requestID;

    req.operation   = ELOS_ASYNC_FILE_READ;
    req.flags       = 0;

    req.write.file   = stream->file;
    req.write.buffer = stream->cache;
    req.write.offset = stream->position;
    req.write.size   = stream->cache_max;

    // printf("cached fwrite pos=0x%zx bytes=%zd\n", req.write.offset, req.write.size);

    requestID = async_submit(&req);
    bool res = async_wait(requestID, &cqe, 0);

    stream->cache_len = 0;

    if (!res) {
        return false;
    }

    if (cqe.error != ELOS_OK) {
        return false;
    }

    return cqe.write.writtenBytes == req.write.size;
}

static size_t cached_read(void* ptr, size_t size, FILE *restrict stream) {
    
    // First make sure we have a cache.
    if (!stream->rcache) {
        // DOOM writes 1 byte at a time when saving game which is very slow without a cache.
        // Not sure what an optimal cache capacity is. A couple of pages will do for now.
        int newMax = 0x2000;
        stream->rcache = malloc(newMax);
        if (stream->rcache) {
            stream->rcache_max = newMax;
            stream->rcache_len = 0;
            stream->rcache_pos = 0;
        }
    }

    // If position changed using fseek then we throw away cache.
    if (stream->rcache_len && stream->position != stream->rcache_pos + stream->rcache_readOffset) {
        stream->rcache_len = 0;
    }

    size_t cachedBytesToRead = 0;
    if (stream->rcache_len) {
        cachedBytesToRead = size;
        if (cachedBytesToRead > stream->rcache_len - stream->rcache_readOffset) {
            cachedBytesToRead = stream->rcache_len - stream->rcache_readOffset;
        }
        memcpy(ptr, stream->rcache + stream->rcache_readOffset, cachedBytesToRead);
        stream->rcache_readOffset += cachedBytesToRead;
        if (stream->rcache_len <= stream->rcache_readOffset) {
            stream->rcache_len = 0; // out of cache.
        }
        if (size - cachedBytesToRead <= 0) {
            return size;
        }
    }
    
    if (stream->rcache && stream->rcache_len == 0 && size - cachedBytesToRead < stream->cache_max) {
        stream->rcache_pos = stream->position;
        ELOS_AsyncRequest req;
        ELOS_AsyncCompletion cqe;
        Async_RequestID requestID;

        req.operation   = ELOS_ASYNC_FILE_READ;
        req.flags       = 0;

        req.read.file   = stream->file;
        req.read.buffer = stream->rcache;
        req.read.offset = stream->rcache_pos;
        req.read.size   = stream->rcache_max;

        requestID = async_submit(&req);
        bool res = async_wait(requestID, &cqe, 0);
        if (!res) {
            return cachedBytesToRead;
        }

        if (cqe.error != ELOS_OK) {
            return cachedBytesToRead;
        }

        stream->rcache_len = cqe.read.readBytes;
        stream->rcache_readOffset = 0;

        memcpy(ptr + cachedBytesToRead, stream->rcache + stream->rcache_readOffset, size - cachedBytesToRead);
        stream->rcache_readOffset += size - cachedBytesToRead;

        u32 readBytes = cachedBytesToRead + size - cachedBytesToRead + cqe.read.readBytes;
        return readBytes;
    } else {
        ELOS_AsyncRequest req;
        ELOS_AsyncCompletion cqe;
        Async_RequestID requestID;

        req.operation   = ELOS_ASYNC_FILE_READ;
        req.flags       = 0;

        req.read.file   = stream->file;
        req.read.buffer = ptr + cachedBytesToRead;
        req.read.offset = stream->position;
        req.read.size   = size - cachedBytesToRead;

        requestID = async_submit(&req);
        bool res = async_wait(requestID, &cqe, 0);
        if (!res) {
            return cachedBytesToRead;
        }
        
        if (cqe.error != ELOS_OK) {
            // Shouldn't be equal to req.ead.size or it would have failed.
            // User detects input vs output size and if they differ
            // then there was an error but they still read some bytes.
            return cachedBytesToRead + cqe.read.readBytes;
        }

        return cachedBytesToRead + cqe.read.readBytes;
    }
}

size_t fread(void* ptr, size_t size, size_t n, FILE *restrict stream) {
    ELOS_Error error;

    // printf("fread pos=0x%zx bytes=0x%zx\n", stream->position, size * n);

    size_t readBytes = cached_read(ptr, size * n, stream);
    stream->position += (readBytes * size) / size;
    return readBytes / size;

    // ELOS_AsyncRequest req;
    // ELOS_AsyncCompletion cqe;
    // Async_RequestID requestID;

    // req.operation   = ELOS_ASYNC_FILE_READ;
    // req.flags       = 0;

    // req.read.file   = stream->file;
    // req.read.buffer = ptr;
    // req.read.offset = stream->position;
    // req.read.size   = size * n;

    // requestID = async_submit(&req);
    // bool res = async_wait(requestID, &cqe, 0);
    // if (!res) {
    //     return 0;
    // }

    // stream->position += cqe.read.readBytes;
    
    // if (cqe.error != ELOS_OK) {
    //     return cqe.read.readBytes;
    // }

    // return cqe.read.readBytes;
}


static bool flush_write_cache(FILE *restrict stream) {
    if (!stream->cache_len) {
        return true;
    }

    ELOS_Error error;

    ELOS_AsyncRequest req;
    ELOS_AsyncCompletion cqe;
    Async_RequestID requestID;

    req.operation   = ELOS_ASYNC_FILE_WRITE;
    req.flags       = 0;

    req.write.file   = stream->file;
    req.write.buffer = stream->cache;
    req.write.offset = stream->cache_pos;
    req.write.size   = stream->cache_len;

    // printf("cached fwrite pos=0x%zx bytes=%zd\n", req.write.offset, req.write.size);

    requestID = async_submit(&req);
    bool res = async_wait(requestID, &cqe, 0);

    stream->cache_len = 0;

    if (!res) {
        return false;
    }

    if (cqe.error != ELOS_OK) {
        return false;
    }

    return cqe.write.writtenBytes == req.write.size;
}

static size_t cached_write(const void* ptr, size_t size, FILE *restrict stream) {
    
    // First make sure we have a cache.
    if (!stream->cache) {
        // DOOM writes 1 byte at a time when saving game which is very slow without a cache.
        // Not sure what an optimal cache capacity is. A couple of pages will do for now.
        int newMax = 0x2000;
        stream->cache = malloc(newMax);
        if (stream->cache) {
            stream->cache_max = newMax;
            stream->cache_len = 0;
            stream->cache_pos = 0;
        }
    }
    

    if (stream->cache) {
        // If position changed using fseek then we mush flush cache at previous position.
        if (stream->cache_len && stream->position != stream->cache_pos + stream->cache_len) {
            flush_write_cache(stream);
        }

        // If we have an empty cache
        if (stream->cache_len == 0 && size <= stream->cache_max) {
            stream->cache_pos = stream->position;
            memcpy(stream->cache, ptr, size);
            stream->cache_len += size;
            return size;
        }

        // If we have stuff in cache which we can append to
        if (stream->cache_len + size <= stream->cache_max) {
            memcpy(stream->cache + stream->cache_len, ptr, size);
            stream->cache_len += size;
            return size;
        } else if (size - stream->cache_len <= stream->cache_max) {
            int bytesToCache = size - stream->cache_len;
            memcpy(stream->cache + stream->cache_len, ptr, bytesToCache);
            stream->cache_len += bytesToCache;

            flush_write_cache(stream);

            int remainingBytes = size - bytesToCache;
            memcpy(stream->cache, ptr + bytesToCache, remainingBytes);
            stream->cache_len = remainingBytes;
            stream->cache_pos = stream->position;

            return size;
        } else {
            flush_write_cache(stream);
        }
    }

    ELOS_Error error;
    ELOS_AsyncCompletion cqe;
    Async_RequestID requestID;
    ELOS_AsyncRequest req;

    req.operation   = ELOS_ASYNC_FILE_WRITE;
    req.flags       = 0;

    req.write.file   = stream->file;
    req.write.buffer = ptr;
    req.write.offset = stream->position;
    req.write.size   = size;

    // printf("UNCACHED fwrite pos=0x%zx bytes=%zd\n", req.write.offset, req.write.size);

    requestID = async_submit(&req);
    bool res = async_wait(requestID, &cqe, 0);
    if (!res) {
        return 0;
    }

    if (cqe.error != ELOS_OK) {
        // Shouldn't be equal to req.write.size
        // or it would have failed.
        // User detects input vs output size and if they differ
        // then there was an error but they still wrote some bytes.
        return cqe.write.writtenBytes;
    }

    return cqe.write.writtenBytes / size;
}


size_t fwrite(const void* ptr, size_t size, size_t n, FILE *restrict stream) {
    if (stream == stderr || stream == stdout) {
        SYS_debug_log(ptr, size * n);
        return n;
    }


    size_t writtenBytes = cached_write(ptr, size * n, stream);
    stream->position += (writtenBytes / size) * size;
    return writtenBytes / size;

    // Below is uncached write if we want in the future:

    // ELOS_Error error;

    // ELOS_AsyncRequest req;
    // ELOS_AsyncCompletion cqe;
    // Async_RequestID requestID;

    // req.operation   = ELOS_ASYNC_FILE_WRITE;
    // req.flags       = 0;

    // req.write.file   = stream->file;
    // req.write.buffer = ptr;
    // req.write.offset = stream->position;
    // req.write.size   = size * n;

    // requestID = async_submit(&req);
    // bool res = async_wait(requestID, &cqe, 0);
    // if (!res) {
    //     return 0;
    // }

    // if (cqe.error != ELOS_OK) {
    //     return cqe.write.writtenBytes;
    // }

    // stream->position += cqe.write.writtenBytes;

    // return cqe.write.writtenBytes;
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

int __isoc99_sscanf(const char* str, const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int r = vsscanf(str, fmt, ap);
    va_end(ap);
    return r;
}

int remove(const char* _path) {
    ELOS_Error error;
    
    GET_ABS_PATH(path, _path);

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

int rename(const char* _oldpath, const char* _newpath) {
    ELOS_Error error;

    GET_ABS_PATH(oldpath, _oldpath);
    GET_ABS_PATH(newpath, _newpath);

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
int mkdir(const char* _path, mode_t mode) {
    GET_ABS_PATH(path, _path);

    ELOS_AsyncRequest req;
    ELOS_AsyncCompletion cqe;
    Async_RequestID requestID;

    req.operation   = ELOS_ASYNC_FILE_MKDIR;
    req.flags       = 0;

    req.mkdir.path = path;

    requestID = async_submit(&req);
    bool res = async_wait(requestID, &cqe, 0);
    if (!res || cqe.error != ELOS_OK) {
        errno = ENOENT;
        printf("mkdir: Could not make %s\n", path);
        return -1;
    }

    return 0;
}


char *getcwd(char* buf, size_t size) {
    snprintf(buf, size, "%s", cwd);
    return buf;
}

int chdir(const char *path) {
    cwd_len = snprintf(cwd, sizeof(cwd), "%s", path);
    return 0;
}


ELOS_Error elos_readdir(const char* _path, u64* cookie, u64* entryCount, ELOS_DirectoryEntry* buffer) {
    
    ELOS_Error error;

    GET_ABS_PATH(path, _path);

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



