/*
    This files describes the available syscalls in ELOS.

    User processes call syscalls. The syscall operates based on the
    process's capabilities.

    Define ELOS_SYSCALL_IMPL to get implementions for functions.

*/

#ifndef ELOS_SYSCALL_INCLUDE
#define ELOS_SYSCALL_INCLUDE

// consider using stdint.h instead
#include "elos/common/types.h"
#include "elos/keycode.h"

typedef enum {
    ELOS_OK = 0,
    ELOS_GENERIC_ERROR,
    ELOS_CAP_DENIED,
    ELOS_INVALID_PARAM,
    ELOS_INVALID_SYSCALL,
    ELOS_INVALID_OPERATION,
    ELOS_IPC_FULL,
} ELOS_Error;

typedef enum {
    ELOS_CAP_HEAP            = (1<<0),
    ELOS_CAP_FILE            = (1<<1),
    ELOS_CAP_MONITOR         = (1<<2),
    ELOS_CAP_THREAD          = (1<<3),
    ELOS_CAP_PROCESS         = (1<<4),
    ELOS_CAP_NETWORK         = (1<<5),
    ELOS_CAP_TIME            = (1<<6),
    ELOS_CAP_SHARED_MEMORY   = (1<<7),
    ELOS_CAP_SERVICE_SERVER  = (1<<8),
    ELOS_CAP_SERVICE_CLIENT  = (1<<9),
    ELOS_CAP_USER_EVENT      = (1<<10),
    ELOS_CAP_ASYNC           = (1<<11),
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

// @TODO Can endpoint handles be reused? Services want a way to map an endpoint to an internal struct.
//   If endpoint is reused then If they can then how does service get an application ID that is unique so
//   others can't d
typedef void* ELOS_ServiceEndpoint;
typedef void* ELOS_SharedMemory;
typedef void* ELOS_UserEventBufferHandle;
typedef void* ELOS_File;

typedef void(*FN_thread_entry)(void* arg);
typedef void* ELOS_ThreadHandle;
typedef u32 ELOS_ThreadID;

typedef u32 ELOS_DeviceID;

typedef enum {
    ELOS_USER_EVENT_CONNECTED, // mouse,keyboard,controllers
    ELOS_USER_EVENT_DISCONNECTED,
    ELOS_USER_EVENT_KEY, // includes normal mouse and controller buttons?
    ELOS_USER_EVENT_MOUSE_MOVE,
    ELOS_USER_EVENT_MOUSE_SCROLL, // contains X and Y component (if X is available)
    ELOS_USER_EVENT_CONTROLLER_LEFT_JOYSTICK,
} ELOS_UserEventType;

typedef struct {
    u32 keycode;
    u32 scancode;
    u32 character;
    u32 value; // zero = released, non-zero = how much it is pressed
    u32 mods;
} ELOS_UserEvent_Key;

typedef struct {
    ELOS_UserEventType type;
    ELOS_DeviceID id;
    union {
        // struct {
        // } connected;
        // struct {
        // } disconnected;
        ELOS_UserEvent_Key key;
    };
} ELOS_UserEvent;

typedef struct {
    // We won't have more than 4 billion events at once so 32-bit integers would work however
    // u64 means we can increment head and tail without worrying about wrap around.
    // unless program runs for a really long time with many events.
    const    u64 maxEvents;
    volatile u64 head; // @TODO reserve, commit head/tail?
    volatile u64 tail;
    ELOS_UserEvent events[];
} ELOS_UserEventBuffer;

#define ELOS_NULL_HANDLE (NULL)


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

    @pre ELOS_CAP_HEAP capability is required.
*/
ELOS_Error SYS_heap_allocate(void** newAddress, u64 size);
ELOS_Error SYS_heap_free(void* oldAddress);
ELOS_Error SYS_heap_reallocate(void** newAddress, u64 size, void* oldAddress);
ELOS_Error SYS_heap_map(void* virtAddress, u64 size);


/*
    Retrieves a frame buffer to the default monitor.

    @pre ELOS_CAP_MONITOR capability is required.

    @param frameBuffer Filled with information.
*/
ELOS_Error SYS_default_monitor(ELOS_FrameBuffer* frameBuffer);


/*
    Tick refers to Real Time Clock or Time Stamp Counter.
    Use with rdtsc to measure elapsed seconds.

    @pre ELOS_CAP_TIME capability is required.

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
    Creates a service for receiving messages from applications that connect.

    @pre ELOS_CAP_SERVICE_SERVER capability is required.
*/
ELOS_Error SYS_service_create(const char* name, ELOS_ServiceEndpoint* endpoint, u64 queueSize);


/*
    Creates a connection to a service. It allows you to send and receive messages
    between processes.

    @pre ELOS_CAP_SERVICE_CLIENT capability is required.
*/
ELOS_Error SYS_service_connect(const char* name, ELOS_ServiceEndpoint* endpoint, u64 queueSize);


/*
    Sends messages to the service channel.

    @pre ELOS_CAP_SERVICE_SERVER or ELOS_CAP_SERVICE_CLIENT capability is required.

    @param endpoint The endpoint to send from.
    @param senderEndpoint Only relevant if endpoint is a servie and not the connection to the service.
    @param data Buffer to send.
    @param size Size of buffer to send.
    @return ELOS_IPC_FULL if service channel is full. ELOS_INVALID_PARAM if endpoint or data pointer are invalid.
*/
ELOS_Error SYS_service_send(ELOS_ServiceEndpoint endpoint, const void* data, u64 size);


/*
    Receive messages from the service channel.

    @pre ELOS_CAP_SERVICE capability is required.

    @param endpoint Endpoint to receive to.
    @param senderHandle Endpoint to receive from.
    @param data Buffer to received message data. NULL if no messages were received. Kernel prepares the buffer and it is valid until next recv call on the same endpoint. 
    @param size Size of received message. 0 if no messages received.
    @param timeout_ns If no messages then function will block for this amount of time.
        -1 to block until message is received.
    @return ELOS_INVALID_PARAM if handle or data pointer are invalid.
*/
ELOS_Error SYS_service_recv(ELOS_ServiceEndpoint endpoint, ELOS_ServiceEndpoint* senderEndpoint, const void** data, u64* size, u64 timeout_ns);


/*
    Allocate memory that can be shared with other processes.

    @pre ELOS_CAP_SHARED_MEMORY capability is required.
*/
ELOS_Error SYS_shared_memory_create(u64 size, ELOS_SharedMemory* handle);


/*
    Share memory with another process. Use service functions to acquire the endpoint.

    @pre ELOS_CAP_SHARED_MEMORY capability is required.
*/
ELOS_Error SYS_shared_memory_grant(ELOS_SharedMemory handle, ELOS_ServiceEndpoint endpoint);


/*
    Information about the shared memory.
    Address is aligned by 4096 bytes (a page).

    @pre ELOS_CAP_SHARED_MEMORY capability is required.
*/
ELOS_Error SYS_shared_memory_info(ELOS_SharedMemory handle, void** buffer, u64* size);


/*
    Requests a ring buffer for user events. The OS fills this buffer and
    old events are overridden.

    @pre ELOS_CAP_USER_EVENT capability is required.
*/
ELOS_Error SYS_request_user_event_buffer(u32 minimumEvents, ELOS_UserEventBuffer** buffer);

/*
    Terminate the process.

    @param exitCode The exit code.
*/
void SYS_exit(int exitCode);

/*
    Terminate the thread. If no more threads then process is also terminated.
*/
void SYS_thread_exit();

/*
    Spawn a thread.

    @param entry  Entry point of the thread.
    @param handle Handle to the thread
*/
ELOS_Error SYS_spawn_thread(FN_thread_entry entry, ELOS_ThreadHandle* handle);

/*
    Join a thread into the thread that spawned it. 

    @param handle Handle to the thread
*/
ELOS_Error SYS_join(ELOS_ThreadHandle handle);

/*
    @return ID of current thread.
*/
ELOS_ThreadID SYS_self();

/*
    Spawn a process.

    @TODO Should we get handle to it so we can wait for it to finish.
          We want to explore unconvential ways to do capabilities/threads/processes.
          Domain execution is the term I use to differentiate from normal processes.

    @param path     Path to the executable.
    @param data     Data to pass to the executable. Process should parse flags from it. ()
    @param data_len Size of the data.
*/
ELOS_Error SYS_spawn_process(const char* path, const char* data, u32 data_len);

// @TODO Spawn a process.

// We have ASYNC operations for these.
// We may provide syscalls for convenience?
// ELOS_Error SYS_file_open(const char* path, ELOS_File* file);
// ELOS_Error SYS_file_close(ELOS_File file);
// ELOS_Error SYS_file_read(ELOS_File file, u64 offset, void* data, u64* size);
// ELOS_Error SYS_file_write(ELOS_File file, u64 offset, const void* data, u64* size);
// ELOS_Error SYS_file_info(ELOS_File file, u64* size);

// ELOS_Error SYS_file_remove(const char* path);
// ELOS_Error SYS_file_rename(const char* old_path, const char* new_path);
// ELOS_Error SYS_file_mkdir(const char* path);


/*
    Asynchonrous operations

    @TODO What to support:
        File operations
        Network operations
        Timer wait, events, signaling?
*/

typedef enum {
    ELOS_ASYNC_KERNEL_POLLING = 0x1,
} ELOS_AsyncCreateFlag;

enum _ELOS_AsyncOperation {
    ELOS_ASYNC_INVALID = 0,
    ELOS_ASYNC_FILE_OPEN = 1,
    ELOS_ASYNC_FILE_CLOSE,
    ELOS_ASYNC_FILE_READ,
    ELOS_ASYNC_FILE_WRITE,
    ELOS_ASYNC_FILE_INFO,
    ELOS_ASYNC_FILE_REMOVE,
    ELOS_ASYNC_FILE_RENAME,
    ELOS_ASYNC_FILE_COPY,
    ELOS_ASYNC_FILE_MKDIR,
    ELOS_ASYNC_FILE_READDIR,
};
typedef u16 ELOS_AsyncOperation;

typedef enum {
    ELOS_FILE_OPEN_FLAG_READ_ONLY = 0x1, // Allows multiple readers on same file.
    ELOS_FILE_OPEN_FLAG_CREATE = 0x2, // Create file if missing
} ELOS_FileOpenFlag;

typedef struct {
    u64  fileSize;
    u32  blockSize;
    bool isDirectory;
    bool readOnly;
    u64  lastWriteTime_us;
} ELOS_FileInfo;

typedef struct {
    char name[63];
    u8   name_len;
    bool isDirectory;
    bool isReadOnly;
    u64  fileSize;
    u64  lastWriteTime_us;
} ELOS_DirectoryEntry;

typedef struct {
    ELOS_AsyncOperation operation;
    u16 flags;
    u32 reserved;
    u64 userData;

    union {
        struct {
            const char* path;
            ELOS_FileOpenFlag flags;
        } open;
        struct {
            ELOS_File file;
        } close;
        struct {
            ELOS_File file;
            u64       offset;
            u64       size;
            void*     buffer;
        } read;
        struct {
            ELOS_File file;
            u64       offset;
            u64       size;
            const void*     buffer;
        } write;
        struct {
            ELOS_File      file;
            ELOS_FileInfo* fileInfo;
        } info;
        struct {
            const char* path;
        } remove;
        struct {
            const char* oldPath;
            const char* newPath;
        } rename;
        struct {
            const char* srcPath;
            const char* dstPath;
        } copy;
        struct {
            const char* path;
        } mkdir;
        struct {
            const char*          path;
            u64                  cookie;
            u64                  maxEntries;
            ELOS_DirectoryEntry* buffer;
        } readdir;
    };
} ELOS_AsyncRequest;

typedef struct {
    ELOS_AsyncOperation operation;
    u16        flags;
    ELOS_Error error;
    u64        userData;
    union {
        struct {
            ELOS_File file;
        } open;
        struct {
            u64 readBytes;
        } read;
        struct {
            u64 writtenBytes;
        } write;
        struct {
            u64 cookie;
            u64 entryCount;
        } readdir;
    };
} ELOS_AsyncCompletion;

typedef struct {
    volatile u64 head;
    volatile u64 tail;
    const    u64 ringMask;
             u32 reserved;
    volatile ELOS_AsyncRequest entries[];
} ELOS_AsyncRequestRing;

typedef struct {
    volatile u64 head;
    volatile u64 tail;
    const    u32 ringMask;
             u32 reserved;
    volatile ELOS_AsyncCompletion entries[];
} ELOS_AsyncCompletionRing;


/*
    Creates two rings for asynronous operations.

    @pre ELOS_CAP_ASYNC capability is required.
*/
ELOS_Error SYS_create_async_rings(u32 maxEntries, ELOS_AsyncCreateFlag flags, ELOS_AsyncRequestRing** requestRing, ELOS_AsyncCompletionRing** completionRing);


/*
    Destroys two rings. Operations in progress are aborted.

    @pre ELOS_CAP_ASYNC capability is required.
*/
ELOS_Error SYS_destroy_async_rings(ELOS_AsyncRequestRing* requestRing, ELOS_AsyncCompletionRing* completionRing);


/*
    Submits entries in the ring for processing by the kernel. Not necessary if
    ring was created with ELOS_ASYNC_KERNEL_POLLING.

    @pre ELOS_CAP_ASYNC capability is required.
*/
ELOS_Error SYS_submit_async_ring(ELOS_AsyncRequestRing* requestRing);


/*
    Waits for kernel to complete an operation.

    @pre ELOS_CAP_ASYNC capability is required.
*/
ELOS_Error SYS_wait_async_ring(ELOS_AsyncCompletionRing* completionRing, u64 timeout_ns);



// @TODO SYS_utc_epoch_time(u64* nanoseconds)
//   Network Time Protocol and DNS to sync the time.


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
    _SYS_REQUEST_USER_EVENT_BUFFER,
    _SYS_CREATE_ASYNC_RING,
    _SYS_DESTROY_ASYNC_RING,
    _SYS_SUBMIT_ASYNC_RING,
    _SYS_WAIT_ASYNC_RING,
    _SYS_EXIT,
} ELOS_SyscallID;


#endif // ELOS_SYSCALL_INCLUDE

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
    register u64 r10 asm ("r10") = (u64)(ARG3);                    \
    asm volatile (                                                 \
        "syscall"                                                  \
        : "=a" (rax)                                               \
        : "a" (ID), "D" (ARG0), "S" (ARG1), "d" (ARG2), "r" (r10)  \
        : "rcx", "r11", "memory"                                   \
    )

#define SYSCALL5(ID, ARG0, ARG1, ARG2, ARG3, ARG4)                            \
    register u64 r10 asm ("r10") = (u64)(ARG3);                               \
    register u64 r8 asm ("r8") = (u64)(ARG4);                                 \
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

    // static u64 tps;
    // if (tps == 0) {
    //     ELOS_Error error = SYS_ticks_per_second(&tps);
    //     if (error != ELOS_OK) {
    //         tps = 2900000000;
    //     }
    // }
    
    // // CPU burning implementation for now
    // u64 start;
    // asm(
    //     "rdtsc\n"
    //     "shl $32, %%rdx\n"
    //     "or %%rdx, %%rax\n"
    //     : "=a" (start)
    //     :
    // );
    // while (1) {
    //     u64 now;
    //     asm(
    //         "rdtsc\n"
    //         "shl $32, %%rdx\n"
    //         "or %%rdx, %%rax\n"
    //         : "=a" (now)
    //         :
    //     );
    //     u64 now_ns = (1000000000 * (now - start)) / tps;
    //     if (now_ns > nanoseconds) {
    //         return;
    //     }
    // }
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

ELOS_Error SYS_service_send(ELOS_ServiceEndpoint endpoint, const void* data, u64 size) {
    ELOS_Error rax;
    SYSCALL3(_SYS_SERVICE_SEND, endpoint, data, size);
    return rax;
}

ELOS_Error SYS_service_recv(ELOS_ServiceEndpoint endpoint, ELOS_ServiceEndpoint* senderEndpoint, const void** data, u64* size, u64 timeout_ns) {
    ELOS_Error rax;
    SYSCALL5(_SYS_SERVICE_RECV, endpoint, senderEndpoint, data, size, timeout_ns);
    return rax;
}

ELOS_Error SYS_shared_memory_create(u64 size, ELOS_SharedMemory* handle) {
    ELOS_Error rax;
    SYSCALL2(_SYS_SHARED_MEMORY_CREATE, size, handle);
    return rax;
}

ELOS_Error SYS_shared_memory_grant(ELOS_SharedMemory handle, ELOS_ServiceEndpoint endpoint) {
    ELOS_Error rax;
    SYSCALL2(_SYS_SHARED_MEMORY_GRANT, handle, endpoint);
    return rax;
}

ELOS_Error SYS_shared_memory_info(ELOS_SharedMemory handle, void** buffer, u64* size) {
    ELOS_Error rax;
    SYSCALL3(_SYS_SHARED_MEMORY_INFO, handle, buffer, size);
    return rax;
}

ELOS_Error SYS_request_user_event_buffer(u32 maxEvents, ELOS_UserEventBuffer** buffer) {
    ELOS_Error rax;
    SYSCALL2(_SYS_REQUEST_USER_EVENT_BUFFER, maxEvents, buffer);
    return rax;
}

ELOS_Error SYS_create_async_rings(u32 maxEntries, ELOS_AsyncCreateFlag flags, ELOS_AsyncRequestRing** requestRing, ELOS_AsyncCompletionRing** completionRing) {
    ELOS_Error rax;
    SYSCALL4(_SYS_CREATE_ASYNC_RING, maxEntries, flags, requestRing, completionRing);
    return rax;
}

ELOS_Error SYS_destroy_async_rings(ELOS_AsyncRequestRing* requestRing, ELOS_AsyncCompletionRing* completionRing) {
    ELOS_Error rax;
    SYSCALL2(_SYS_DESTROY_ASYNC_RING, requestRing, completionRing);
    return rax;
}

ELOS_Error SYS_submit_async_ring(ELOS_AsyncRequestRing* requestRing) {
    ELOS_Error rax;
    SYSCALL1(_SYS_SUBMIT_ASYNC_RING, requestRing);
    return rax;
}

ELOS_Error SYS_wait_async_ring(ELOS_AsyncCompletionRing* completionRing, u64 timeout_ns) {
    ELOS_Error rax;
    SYSCALL2(_SYS_WAIT_ASYNC_RING, completionRing, timeout_ns);
    return rax;
}

void SYS_exit(int exitCode) {
    ELOS_Error rax;
    SYSCALL1(_SYS_EXIT, exitCode);
}


#endif // ELOS_SYSCALL_IMPL


