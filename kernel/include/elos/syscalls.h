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
} ELOS_Error;

typedef enum {
    GLOBAL_CAP_HEAP     = (1<<0),
    GLOBAL_CAP_FILE     = (1<<1),
    GLOBAL_CAP_MONITOR  = (1<<2),
    GLOBAL_CAP_THREAD   = (1<<3),
    GLOBAL_CAP_PROCESS  = (1<<4),
    GLOBAL_CAP_NETWORK  = (1<<5),
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







#endif // ELOS_SYSCALL_IMPL


