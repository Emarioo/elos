/*
    This files describes the available syscalls in ELOS.

    User processes call syscalls. The syscall operates based on the
    process's capabilities.

    Define ELOS_SYSCALL_IMPL to get implementions for functions.

*/

#ifndef _ELOS_SYSCALL_INCLUDE
#define _ELOS_SYSCALL_INCLUDE

// consider using stdint.h instead
#include "elos/common/types.h"

typedef enum {
    ELOS_OK = 0,
    ELOS_GENERIC_ERROR,
    ELOS_CAP_DENIED,
    ELOS_INVALID_PARAM,
    ELOS_INVALID_SYSCALL,
    ELOS_IPC_FULL,
} ELOS_Error;

typedef enum {
    GLOBAL_CAP_HEAP            = (1<<0),
    GLOBAL_CAP_FILE            = (1<<1),
    GLOBAL_CAP_MONITOR         = (1<<2),
    GLOBAL_CAP_THREAD          = (1<<3),
    GLOBAL_CAP_PROCESS         = (1<<4),
    GLOBAL_CAP_NETWORK         = (1<<5),
    GLOBAL_CAP_TIME            = (1<<6),
    GLOBAL_CAP_SHARED_MEMORY   = (1<<7),
    GLOBAL_CAP_SERVICE_SERVER  = (1<<8),
    GLOBAL_CAP_SERVICE_CLIENT  = (1<<9),
} ELOS_GlobalCapability;


typedef struct {
    // @TODO Do we need version/size for backwards and forward compatibility?

    // General capability of different categories
    u64 globalCapabilities;

    //#### Heap capabilities ####
    u64 heap_limit;

    //#### File System capabilities ####


    // network
    // process loading
    // thread creation
    // display
    // timing
    
} ELOS_Capabilities;



typedef struct {
    u32  width;
    u32  height;
    u32  size;
    u32  pixels_per_scan_line;
    u32* pixels;
} ELOS_FrameBuffer;

typedef void* ELOS_ServiceEndpoint;
typedef void* ELOS_SharedMemoryHandle;


/*
    Returns capabilities of process.

    @param capabilities Filled with information.
*/
void SYS_capabilites(ELOS_Capabilities* capabilities);


/*
    Asks the OS for capabilities. Check the in parameter which capabilities
    where accepted. Once they have been accepted you can safely use them.
    Once a capability has been accepted you can never lose it.

    @param capabilities Zero all but the capabilities you want to request.
    On return the accepted capabilities remain non-zero and denied ones become zero.
*/
void SYS_request_capabilites(ELOS_Capabilities* capabilities);


/*
    Sends text to OS. It may print it to serial out, to frame buffer or do nothing.

    @pre No capability required.
*/
void SYS_debug_log(const char* text, u32 length);


/*
    Allocates memory from the heap.

    @pre GLOBAL_CAP_HEAP capability is required.
*/
ELOS_Error SYS_heap_allocate(void** newAddress, u64 size);
ELOS_Error SYS_heap_free(void* oldAddress);
ELOS_Error SYS_heap_reallocate(void** newAddress, u64 size, void* oldAddress);
ELOS_Error SYS_heap_map(void* virtAddress, u64 size);


/*
    Retrieves a frame buffer to the default monitor.

    @pre GLOBAL_CAP_MONITOR capability is required.

    @param frameBuffer Filled with information.
*/
ELOS_Error SYS_default_monitor(ELOS_FrameBuffer* frameBuffer);


/*
    Tick refers to Real Time Clock or Time Stamp Counter.
    Use with rdtsc to measure elapsed seconds.

    @pre GLOBAL_CAP_TIME capability is required.

    @param tps Filled with information.
*/
ELOS_Error SYS_ticks_per_second(u64* tps);


/*
    Sleeps for an amount of time. Can also be used
    to yield the process and reschedule another.

    @pre No capability required.

    @param nanoseconds Amount of time to sleep. Yields process if 0.
*/
void SYS_sleep_ns(u64 nanoseconds);


/*
    @pre GLOBAL_CAP_SERVICE_SERVER capability is required.
*/
ELOS_Error SYS_service_create(const char* name, ELOS_ServiceEndpoint* endpoint, u64 queueSize);


/*
    @pre GLOBAL_CAP_SERVICE_CLIENT capability is required.
*/
ELOS_Error SYS_service_connect(const char* name, ELOS_ServiceEndpoint* endpoint, u64 queueSize);


/*
    Sends messages to the service channel.

    @pre GLOBAL_CAP_SERVICE_SERVER or GLOBAL_CAP_SERVICE_CLIENT capability is required.

    @param endpoint The endpoint to send from.
    @param senderEndpoint Only relevant if endpoint is a servie and not the connection to the service.
    @param data Buffer to send.
    @param size Size of buffer to send.
    @return ELOS_IPC_FULL if service channel is full. ELOS_INVALID_PARAM if endpoint or data pointer are invalid.
*/
ELOS_Error SYS_service_send(ELOS_ServiceEndpoint endpoint, const u8* data, u64 size);


/*
    Receive messages from the service channel.

    @pre GLOBAL_CAP_SERVICE capability is required.

    @param endpoint Endpoint to receive to.
    @param senderHandle Endpoint to receive from.
    @param data Buffer to received message data. NULL if no messages were received. Kernel prepares the buffer and it is valid until next recv call on the same endpoint. 
    @param size Size of received message. 0 if no messages received.
    @param timeout_ns If no messages then function will block for this amount of time.
        -1 to block until message is received.
    @return ELOS_INVALID_PARAM if handle or data pointer are invalid.
*/
ELOS_Error SYS_service_recv(ELOS_ServiceEndpoint endpoint, ELOS_ServiceEndpoint* senderEndpoint, const u8** data, u64* size, u64 timeout_ns);


/*
    Allocate memory that can be shared with other processes.

    @pre GLOBAL_CAP_SHARED_MEMORY capability is required.
*/
ELOS_Error SYS_shared_memory_create(u64 size, ELOS_SharedMemoryHandle* handle);


/*
    Share memory with another process. Use service functions to acquire the endpoint.

    @pre GLOBAL_CAP_SHARED_MEMORY capability is required.
*/
ELOS_Error SYS_shared_memory_grant(ELOS_SharedMemoryHandle handle, ELOS_ServiceEndpoint endpoint);


/*
    Information about the shared memory.

    @pre GLOBAL_CAP_SHARED_MEMORY capability is required.
*/
ELOS_Error SYS_shared_memory_info(ELOS_SharedMemoryHandle handle, void** buffer, u64* size);




// @TODO Auto-generate stuff below?


typedef enum {
    // Zero is reserved for invalid.
    _SYS_CAPABILITIES = 1,
    _SYS_REQUEST_CAPABILITIES,
    _SYS_DEBUG_LOG,
    _SYS_HEAP_ALLOCATE,
    _SYS_HEAP_FREE,
    _SYS_HEAP_REALLOCATE,
    _SYS_HEAP_MAP,
    _SYS_DEFAULT_MONITOR,
    _SYS_TICKS_PER_SECOND,
    _SYS_SLEEP_NS,
    _SYS_SERVICE_CREATE,
    _SYS_SERVICE_CONNECT,
    _SYS_SERVICE_SEND,
    _SYS_SERVICE_RECV,
    _SYS_SHARED_MEMORY_CREATE,
    _SYS_SHARED_MEMORY_GRANT,
    _SYS_SHARED_MEMORY_INFO,
} ELOS_SyscallID;


#endif // _ELOS_SYSCALL_INCLUDE

#ifdef ELOS_SYSCALL_IMPL


#define SYSCALL0(ID)              \
    asm volatile (                \
        "syscall"                 \
        : "=a" (rax)              \
        : "a" (ID)                \
        : "rcx", "r11", "memory"  \
    );

#define SYSCALL1(ID, ARG0)        \
    asm volatile (                \
        "syscall"                 \
        : "=a" (rax)              \
        : "a" (ID), "D" (ARG0)    \
        : "rcx", "r11", "memory"  \
    );

#define SYSCALL2(ID, ARG0, ARG1)            \
    asm volatile (                          \
        "syscall"                           \
        : "=a" (rax)                        \
        : "a" (ID), "D" (ARG0), "S" (ARG1)  \
        : "rcx", "r11", "memory"            \
    );
    
#define SYSCALL3(ID, ARG0, ARG1, ARG2)                  \
    asm volatile (                                      \
        "syscall"                                       \
        : "=a" (rax)                                    \
        : "a" (ID), "D" (ARG0), "S" (ARG1), "d" (ARG2)  \
        : "rcx", "r11", "memory"                        \
    )

#define SYSCALL4(ID, ARG0, ARG1, ARG2, ARG3)                       \
    register u64 r10 asm ("r10") = (u64)(ARG3);                           \
    asm volatile (                                                 \
        "syscall"                                                  \
        : "=a" (rax)                                               \
        : "a" (ID), "D" (ARG0), "S" (ARG1), "d" (ARG2), "r" (r10)  \
        : "rcx", "r11", "memory"                                   \
    )

#define SYSCALL5(ID, ARG0, ARG1, ARG2, ARG3, ARG4)                            \
    register u64 r10 asm ("r10") = (u64)(ARG3);                                      \
    register u64 r8 asm ("r8") = (u64)(ARG4);                                        \
    asm volatile (                                                            \
        "syscall"                                                             \
        : "=a" (rax)                                                          \
        : "a" (ID), "D" (ARG0), "S" (ARG1), "d" (ARG2), "r" (r10), "r" (r8)   \
        : "rcx", "r11", "memory"                                              \
    )

void SYS_capabilites(ELOS_Capabilities* capabilities)  {
    ELOS_Error rax;
    SYSCALL1(_SYS_CAPABILITIES, capabilities);
}

void SYS_request_capabilites(ELOS_Capabilities* capabilities) {
    ELOS_Error rax;
    SYSCALL1(_SYS_REQUEST_CAPABILITIES, capabilities);
}

void SYS_debug_log(const char* text, u32 length) {
    ELOS_Error rax;
    SYSCALL2(_SYS_DEBUG_LOG, text, length);
}

ELOS_Error SYS_heap_allocate(void** newAddress, u64 size) {
    ELOS_Error rax;
    SYSCALL2(_SYS_HEAP_ALLOCATE, newAddress, size);
    return rax;
}

ELOS_Error SYS_heap_free(void* oldAddress) {
    ELOS_Error rax;
    SYSCALL1(_SYS_HEAP_FREE, oldAddress);
    return rax;
}

ELOS_Error SYS_heap_reallocate(void** newAddress, u64 size, void* oldAddress) {
    ELOS_Error rax;
    SYSCALL3(_SYS_HEAP_REALLOCATE, newAddress, size, oldAddress);
    return rax;
}

ELOS_Error SYS_heap_map(void* virtAddress, u64 size) {
    ELOS_Error rax;
    SYSCALL2(_SYS_HEAP_MAP, virtAddress, size);
    return rax;
}

ELOS_Error SYS_default_monitor(ELOS_FrameBuffer* frameBuffer) {
    ELOS_Error rax;
    SYSCALL1(_SYS_DEFAULT_MONITOR, frameBuffer);
    return rax;
}

ELOS_Error SYS_ticks_per_second(u64* tps) {
    ELOS_Error rax;
    SYSCALL1(_SYS_TICKS_PER_SECOND, tps);
    return rax;
}

void SYS_sleep_ns(u64 nanoseconds) {
    ELOS_Error rax;
    SYSCALL1(_SYS_SLEEP_NS, nanoseconds);
}

ELOS_Error SYS_service_create(const char* name, ELOS_ServiceEndpoint* endpoint, u64 queueSize) {
    ELOS_Error rax;
    SYSCALL3(_SYS_SERVICE_CREATE, name, endpoint, queueSize);
    return rax;
}

ELOS_Error SYS_service_connect(const char* name, ELOS_ServiceEndpoint* endpoint, u64 queueSize) {
    ELOS_Error rax;
    SYSCALL3(_SYS_SERVICE_CONNECT, name, endpoint, queueSize);
    return rax;
}

ELOS_Error SYS_service_send(ELOS_ServiceEndpoint endpoint, const u8* data, u64 size) {
    ELOS_Error rax;
    SYSCALL3(_SYS_SERVICE_SEND, endpoint, data, size);
    return rax;
}

ELOS_Error SYS_service_recv(ELOS_ServiceEndpoint endpoint, ELOS_ServiceEndpoint* senderEndpoint, const u8** data, u64* size, u64 timeout_ns) {
    ELOS_Error rax;
    SYSCALL5(_SYS_SERVICE_RECV, endpoint, senderEndpoint, data, size, timeout_ns);
    return rax;
}

ELOS_Error SYS_shared_memory_create(u64 size, ELOS_SharedMemoryHandle* handle) {
    ELOS_Error rax;
    SYSCALL2(_SYS_SHARED_MEMORY_CREATE, size, handle);
    return rax;
}

ELOS_Error SYS_shared_memory_grant(ELOS_SharedMemoryHandle handle, ELOS_ServiceEndpoint endpoint) {
    ELOS_Error rax;
    SYSCALL2(_SYS_SHARED_MEMORY_GRANT, handle, endpoint);
    return rax;
}

ELOS_Error SYS_shared_memory_info(ELOS_SharedMemoryHandle handle, void** buffer, u64* size)  {
    ELOS_Error rax;
    SYSCALL3(_SYS_SHARED_MEMORY_INFO, handle, buffer, size);
    return rax;
}






#endif // ELOS_SYSCALL_IMPL


