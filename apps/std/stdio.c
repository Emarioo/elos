
#include "elos/common/string.h"

#include "elos/syscalls.h"

#include <stdarg.h>
#include "async_io.h"

#include "stdlib.h"
#include "stdio.h"


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

