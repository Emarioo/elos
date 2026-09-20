
#include "stdnet.h"

#include "stdio.h"
#include "string.h"
#include "stdlib.h"

#include "async_io.h"


ELOS_Error net_open(const ELOS_Net_Address* address, ELOS_Net_Handle* handle) {
    ELOS_Error error;
    
    ELOS_AsyncRequest req = {0};
    ELOS_AsyncCompletion cqe;
    Async_RequestID requestID;

    req.operation   = ELOS_ASYNC_NET_OPEN;
    req.flags       = 0;

    req.net_open.address = address;

    requestID = async_submit(&req);
    bool res = async_wait(requestID, &cqe, 0);
    if (!res) {
        printf("-1 from async_wait\n");
        return ELOS_ERR_UNKNOWN;
    }

    *handle = cqe.net_open.handle;
    return cqe.error;
}

ELOS_Error net_close(ELOS_Net_Handle handle) {
    ELOS_Error error;
    
    ELOS_AsyncRequest req = {0};
    ELOS_AsyncCompletion cqe;
    Async_RequestID requestID;

    req.operation   = ELOS_ASYNC_NET_CLOSE;
    req.flags       = 0;

    req.net_close.handle = handle;

    requestID = async_submit(&req);
    bool res = async_wait(requestID, &cqe, 0);
    if (!res) {
        printf("-1 from async_wait\n");
        return ELOS_ERR_UNKNOWN;
    }

    return cqe.error;
}

ELOS_Error net_write(ELOS_Net_Handle handle, const ELOS_Net_Address* address, const void* data, u32 size) {
    ELOS_Error error;
    
    ELOS_AsyncRequest req = {0};
    ELOS_AsyncCompletion cqe;
    Async_RequestID requestID;

    req.operation   = ELOS_ASYNC_NET_WRITE;
    req.flags       = 0;

    req.net_write.handle = handle;
    req.net_write.address = address;
    req.net_write.data = data;
    req.net_write.size = size;

    requestID = async_submit(&req);
    bool res = async_wait(requestID, &cqe, 0);
    if (!res) {
        printf("-1 from async_wait\n");
        return ELOS_ERR_UNKNOWN;
    }

    return cqe.error;
}

ELOS_Error net_read(ELOS_Net_Handle handle, ELOS_Net_Address* address, void* buffer, u32* bufferSize, u64 timeout_ns) {
    ELOS_Error error;
    
    ELOS_AsyncRequest req = {0};
    ELOS_AsyncCompletion cqe;
    Async_RequestID requestID;

    req.operation   = ELOS_ASYNC_NET_READ;
    req.flags       = 0;

    req.net_read.handle = handle;
    req.net_read.address = address;
    req.net_read.buffer = buffer;
    req.net_read.bufferSize = *bufferSize;
    req.net_read.timeout_ns = timeout_ns;

    requestID = async_submit(&req);
    bool res = async_wait(requestID, &cqe, 0);
    if (!res) {
        printf("-1 from async_wait\n");
        return ELOS_ERR_UNKNOWN;
    }

    *bufferSize = cqe.net_read.readBytes;

    return cqe.error;
}

u32 net_ipv4_from_str(const char* address) {
    char* string = (char*)address;
    u32 num;
    num  = (u32)strtol(string  , &string, 10);
    num |= (u32)strtol(string+1, &string, 10) << 8;
    num |= (u32)strtol(string+1, &string, 10) << 16;
    num |= (u32)strtol(string+1, &string, 10) << 24;
    return num;
}

const char* net_ipv4_str(u32 address) {
    static char buffer[50]; // @TODO Make it thread safe
    snprintf(buffer, sizeof(buffer), "%u.%u.%u.%u", 
        address & 0xFF, (address >> 8) & 0xFF, (address >> 16) & 0xFF, address >> 24
    );
    return buffer;
}
