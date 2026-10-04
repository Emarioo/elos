/*
    This files describes the available syscalls in ELOS.

    User processes call syscalls. The syscall operates based on the
    process's permissions.

    Define ELOS_SYSCALL_IMPL to get implementions for functions.

    Note: Rules for consistency

        - "size" ALWAYS refers to bytes.
            int bufferSize;
        - "count", "amount", "length" ALWAYS refers to elements and NEVER bytes.
           Except for 'char' type which happens to be a byte as well.
            int stringLength;
            int ringCount;
        - "index" refers to a zero-based element position.
        - "offset" always refers to a byte offset.

*/

#ifndef ELOS_SYSCALL_INCLUDE
#define ELOS_SYSCALL_INCLUDE

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "elos/keycode.h"

#if defined(__x86_64__)
#define ELOS_PADDING
#else
#define ELOS_LINETHING(X,Y) X##Y
#define ELOS_LINETHING2(Y) ELOS_LINETHING(_reserved, Y)
#define ELOS_PADDING uint32_t ELOS_LINETHING2(__LINE__);
#endif


typedef enum {
    ELOS_OK = 0,

    //####  Normal errors  ####

    // Reserved, it means OS accidently used 'true' instead of ELOS_OK. What a silly OS am I right?
    ELOS_ERR_RESERVED = 1,

    // No specific information about the error is available.
    // We should go out of our way to rid codebase from these.
    // But there is a place for them.
    ELOS_ERR_UNKNOWN,

    // Syscall number was not known by the OS. Also used in AsyncCompletion.error if operation was invalid.
    ELOS_ERR_INVALID_PARAM,
    ELOS_ERR_INVALID_SYSCALL,
    ELOS_ERR_PERMISSION_DENIED,

    ELOS_ERR_NOT_FOUND,
    ELOS_ERR_BUSY,
    ELOS_ERR_TIMEOUT,

    ELOS_ERR_OUT_OF_BOUNDS,


    // ELOS_ERR_IPC_FULL,

    ELOS_ERR_UNSUPPORTED_AUDIO_FORMAT,
    ELOS_ERR_BUFFER_SIZE_NOT_FRAME_ALIGNED,
    ELOS_ERR_BUFFER_SIZE_TOO_BIG,
    ELOS_ERR_BUFFER_SIZE_TOO_SMALL,
    ELOS_ERR_NO_AUDIO_DEVICE,
    
} ELOS_Error;

typedef enum {
    ELOS_PERM_INVALID,

    ELOS_PERM_HEAP_LIMIT,
    ELOS_PERM_HEAP_ALLOC_EXEC,
    ELOS_PERM_HEAP_ALLOC_SHARED,

    ELOS_PERM_THREAD_LIMIT,

    // @SECURITY If a program copies itself to some path and modifies ELF slightly to get new hash
    //   then it gets default permissions. If process spawn is default permission then it can spawn
    //   itself infintelty and get infinite storage and memory quota.
    //   Hence: ELOS_PERM_PROCESS_SPAWN IS NOT ALLOWED AS DEFAULT PERMISSION.
    ELOS_PERM_PROCESS_SPAWN, 
    ELOS_PERM_PROCESS_KILL,

    ELOS_PERM_DISK,
    ELOS_PERM_USER_EVENT,
    ELOS_PERM_AUDIO,
    ELOS_PERM_MONITOR,

    ELOS_PERM_FILESYSTEM,
    ELOS_PERM_FILESYSTEM_FULL_ACCESS,
    ELOS_PERM_FILESYSTEM_STORAGE_LIMIT,

    ELOS_PERM_NETWORK,
    ELOS_PERM_NETWORK_FULL_ACCESS,

} ELOS_PermissionType;


typedef uint32_t ELOS_ProcessID;
typedef uint32_t ELOS_ThreadID;


typedef struct {
    // @TODO Do we need version/size for backwards and forward compatibility?

    // General permission of different categories
    uint64_t globalPermissions;

    //#### Heap permissions ####
    uint64_t heap_limit;

    //#### File System permissions ####


    // network
    // process loading
    // thread creation
    // display
    // timing
    
} ELOS_Permissions;



typedef struct {
    uint32_t  width;
    uint32_t  height;
    uint32_t  size;
    uint32_t  pixels_per_scan_line;
    uint32_t* pixels;
    ELOS_PADDING
} ELOS_FrameBuffer;

// @TODO Can endpoint handles be reused? Services want a way to map an endpoint to an internal struct.
//   If endpoint is reused then If they can then how does service get an application ID that is unique so
//   others can't d
typedef void* ELOS_ServiceEndpoint;
typedef void* ELOS_SharedMemoryHandle;
typedef void* ELOS_UserEventBufferHandle;
typedef void* ELOS_File;

typedef void(*FN_thread_entry)(void* arg);
typedef void* ELOS_ThreadHandle;
typedef void* ELOS_ProcessHandle;

typedef uint32_t ELOS_DeviceID;

typedef struct NET_Device NET_Device;

typedef enum {
    ELOS_USER_EVENT_CONNECTED, // mouse,keyboard,controllers
    ELOS_USER_EVENT_DISCONNECTED,
    ELOS_USER_EVENT_KEY, // includes normal mouse and controller buttons?
    ELOS_USER_EVENT_MOUSE_MOVE,
    ELOS_USER_EVENT_MOUSE_SCROLL, // contains X and Y component (if X is available)
    ELOS_USER_EVENT_CONTROLLER_LEFT_JOYSTICK,
} ELOS_UserEventType;

typedef struct {
    uint32_t keycode;
    uint32_t scancode;
    uint32_t character;
    uint32_t value; // zero = released, non-zero = how much it is pressed
    uint32_t mods;
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
    const    uint32_t maxEvents;
    volatile uint32_t head; // @TODO reserve, commit head/tail?
    volatile uint32_t tail;
    ELOS_UserEvent events[];
} ELOS_UserEventBuffer;

#define ELOS_NULL_HANDLE (NULL)



/*
    Returns error in string form.
*/
const char* elos_error(ELOS_Error err);


/*
    Returns permissions of process.

    @param permissions Filled with information.
*/
void SYS_permissions(ELOS_Permissions* permissions);


/*
    Asks the OS for permissions. Check the in parameter which permissions
    where accepted. Once they have been accepted you can safely use them.
    Once a permission has been accepted you can never lose it.

    @param permissions Zero all but the permissions you want to request.
    On return the accepted permissions remain non-zero and denied ones become zero.
*/
void SYS_request_permissions(ELOS_Permissions* permissions);


/*
    Sends text to OS. It may print it to serial out, to frame buffer or do nothing.

    @pre No permission required.
*/
void SYS_debug_log(const char* text, uint32_t length);


typedef enum {
    ELOS_SYSOP_SHUTDOWN,
} ELOS_System_Operation;

/*
    Special operations.

    @pre ELOS_PERM_SYSTEM_OP permission is required.
*/
ELOS_Error SYS_system_operation(ELOS_System_Operation operation, uint8_t* data, uint32_t size);


typedef enum ELOS_Heap_Protection {
    ELOS_HEAP_PROT_NONE  = 0x0,
    ELOS_HEAP_PROT_READ  = 0x1,
    ELOS_HEAP_PROT_WRITE = 0x2,
    ELOS_HEAP_PROT_EXEC  = 0x4,
} ELOS_Heap_Protection;

/*
    Allocates memory from the heap.

    @pre ELOS_PERM_HEAP permission is required.
*/
ELOS_Error SYS_heap_allocate(void** newAddress, size_t size);
ELOS_Error SYS_heap_free(void* oldAddress);
ELOS_Error SYS_heap_reallocate(void** newAddress, size_t size, void* oldAddress);
ELOS_Error SYS_heap_map(void* virtAddress, size_t size, ELOS_Heap_Protection protection);
ELOS_Error SYS_heap_protect(void* virtAddress, size_t size, ELOS_Heap_Protection protection);


/*
    Retrieves a frame buffer to the default monitor.

    @pre ELOS_PERM_MONITOR permission is required.

    @param frameBuffer Filled with information.
*/
ELOS_Error SYS_default_monitor(ELOS_FrameBuffer* frameBuffer);


/*
    Tick refers to Real Time Clock or Time Stamp Counter.
    Use with rdtsc to measure elapsed seconds.

    @pre No permission required.

    @param tps Filled with information.
*/
ELOS_Error SYS_ticks_per_second(uint64_t* tps);


/*
    Sleeps for an amount of time. Can also be used
    to yield the process and reschedule another.

    @pre No permission required.

    @param nanoseconds Amount of time to sleep. Yields process if 0.
*/
void SYS_sleep_ns(uint64_t nanoseconds);


// /*
//     Creates a service for receiving messages from applications that connect.

//     @pre ELOS_PERM_SERVICE_SERVER permission is required.
// */
// ELOS_Error SYS_service_create(const char* name, ELOS_ServiceEndpoint* endpoint, uint32_t queueSize);


// /*
//     Creates a connection to a service. It allows you to send and receive messages
//     between processes.

//     @pre ELOS_PERM_SERVICE_CLIENT permission is required.
// */
// ELOS_Error SYS_service_connect(const char* name, ELOS_ServiceEndpoint* endpoint, uint32_t queueSize);


// /*
//     Sends messages to the service channel.

//     @pre ELOS_PERM_SERVICE_SERVER or ELOS_PERM_SERVICE_CLIENT permission is required.

//     @param endpoint The endpoint to send from.
//     @param senderEndpoint Only relevant if endpoint is a servie and not the connection to the service.
//     @param data Buffer to send.
//     @param size Size of buffer to send.
//     @return ELOS_IPC_FULL if service channel is full. ELOS_INVALID_PARAM if endpoint or data pointer are invalid.
// */
// ELOS_Error SYS_service_send(ELOS_ServiceEndpoint endpoint, const void* data, uint32_t size);


// /*
//     Receive messages from the service channel.

//     @pre ELOS_PERM_SERVICE permission is required.

//     @param endpoint Endpoint to receive to.
//     @param senderHandle Endpoint to receive from.
//     @param data Buffer to received message data. NULL if no messages were received. Kernel prepares the buffer and it is valid until next recv call on the same endpoint. 
//     @param size Size of received message. 0 if no messages received.
//     @param timeout_ns If no messages then function will block for this amount of time.
//         -1 to block until message is received.
//     @return ELOS_INVALID_PARAM if handle or data pointer are invalid.
// */
// ELOS_Error SYS_service_recv(ELOS_ServiceEndpoint endpoint, ELOS_ServiceEndpoint* senderEndpoint, const void** data, uint32_t* size, uint64_t timeout_ns);


/*
    Allocate memory that can be shared with other processes.

    @pre ELOS_PERM_SHARED_MEMORY permission is required.
*/
ELOS_Error SYS_shared_memory_create(size_t size, ELOS_SharedMemoryHandle* handle);


/*
    Share memory with another process. Use service functions to acquire the endpoint.

    @pre ELOS_PERM_SHARED_MEMORY permission is required.
*/
ELOS_Error SYS_shared_memory_grant(ELOS_SharedMemoryHandle handle, ELOS_ProcessID processID);


/*
    Information about the shared memory.
    Address is aligned by 4096 bytes (a page).

    @pre ELOS_PERM_SHARED_MEMORY permission is required.
*/
ELOS_Error SYS_shared_memory_info(ELOS_SharedMemoryHandle handle, void** buffer, size_t* size);


/*
    Requests a ring buffer for user events. The OS fills this buffer and
    old events are overridden.

    @pre ELOS_PERM_USER_EVENT permission is required.
*/
ELOS_Error SYS_request_user_event_buffer(uint32_t minimumEvents, ELOS_UserEventBuffer** buffer);

/*
    Terminate the process.

    @param exitCode The exit code.
*/
void SYS_exit(int exitCode);

/*
    Terminate the thread. If no more threads then process is also terminated.
*/
void SYS_exit_thread();

/*
    Spawn a thread.

    @param entry  Entry point of the thread.
    @param handle Handle to the thread
*/
ELOS_Error SYS_spawn_thread(FN_thread_entry entry, ELOS_ThreadHandle* handle);

/*
    Wait for a thread to finish. Cannot join itself.

    @param handle Handle to the thread
*/
ELOS_Error SYS_join_thread(ELOS_ThreadHandle handle);

/*
    Retrieve ID of thread. Pass NULL for current thread.

    @return ID of thread.
*/
ELOS_ThreadID SYS_thread_id(ELOS_ThreadHandle handle);

/*
    Spawn a process.

    @TODO Should we get handle to it so we can wait for it to finish.
          We want to explore unconvential ways to do permissions/threads/processes.
          Domain execution is the term I use to differentiate from normal processes.

    @param path     Path to the executable.
    @param data     Data to pass to the executable. Process should parse flags from it. ()
    @param data_len Size of the data.
*/
ELOS_Error SYS_spawn_process(const char* path, const char* data, uint32_t data_len);

ELOS_Error SYS_kill_process(ELOS_ProcessHandle handle);
ELOS_Error SYS_kill_process_by_name(const char* name);


/*
    Retrieve ID of process.

    @return ID of process.
*/
ELOS_ProcessID SYS_process_id();



typedef void* ELOS_AudioDevice;

#define ELOS_INVALID_AUDIO_DEVICE NULL


typedef enum {
    ELOS_AUDIO_8BIT_PCM,
    ELOS_AUDIO_16BIT_PCM,
    ELOS_AUDIO_32BIT_PCM,
    ELOS_AUDIO_32BIT_FLOAT,
} ELOS_AudioSampleFormat;

#define ELOS_BYTES_PER_AUDIO_SAMPLE(X) ( (X) == ELOS_AUDIO_32BIT_FLOAT ? 4 : (1 << (X)) )

typedef struct {
    uint32_t sampleRate;
    uint8_t  channels;
    ELOS_AudioSampleFormat sampleFormat;
} ELOS_AudioFormat;

typedef struct {
    char name[32];
} ELOS_AudioDeviceInfo;

typedef struct {
    uint32_t head;
    uint32_t tail;
    uint32_t size;
    uint8_t  data[];
} ELOS_AudioBuffer;

/*
    Returns the default audio device.

    @param device The returned default device.

    @pre ELOS_PERM_AUDIO is required.

    @exception ELOS_NO_AUDIO_DEVICE No audio device.
*/
ELOS_Error SYS_default_audio(ELOS_AudioDevice* device);

// @TODO Provide audio device enumeration functions.

/*
    Returns information about the audio device such as name and audio format.

    @param device The device to get information from.
    @param info The information of the device.

    @pre ELOS_PERM_AUDIO is required.

    @exception ELOS_ERR_UNKNOWN No audio device.
*/
ELOS_Error SYS_audio_info(ELOS_AudioDevice device, ELOS_AudioDeviceInfo* info);

/*
    Returns an audio sample buffer for the audio device. Samples written to the buffer will be
    transferred to the hardware device. This will lock the audio device. Other create_buffer calls by
    this or other processes will fail with ELOS_BUSY.

    @param device The device to make a buffer for.
    @param bufferSize Size in bytes of the buffer.
    @param buffer The buffer to write audio samples to. The kernel will transfer them to the audio device.

    @pre ELOS_PERM_AUDIO is required.

    @exception ELOS_UNSUPPORTED_AUDIO_FORMAT ELOS_BUSY
    
*/
ELOS_Error SYS_create_audio_buffer(ELOS_AudioDevice device, ELOS_AudioFormat* format, uint32_t bufferSize, ELOS_AudioBuffer** buffer);

/*
    Destroys and frees audio buffer.

    @param device The device the buffer is from.
    @param buffer The buffer to destroy.

    @pre ELOS_PERM_AUDIO is required.

    @exception ELOS_UNSUPPORTED_AUDIO_FORMAT
    
*/
ELOS_Error SYS_destroy_audio_buffer(ELOS_AudioDevice device, ELOS_AudioBuffer* buffer);

// typedef enum {
//     AUDIO_IOCTL_PLAY,
//     AUDIO_IOCTL_STOP,
// } ELOS_AudioOperation;

/*
    Perform an operation on the audio device.

    @param device The device to perform operation on.
    @param operation The action to perform.
    @param value Specific to the operation. Some operations needs no value and ignores it.
*/
// ELOS_Error SYS_audio_control(ELOS_AudioDevice device, ELOS_AudioOperation operation, size_t value);


// We have ASYNC operations for these.
// We may provide syscalls for convenience?
// ELOS_Error SYS_file_open(const char* path, ELOS_File* file);
// ELOS_Error SYS_file_close(ELOS_File file);
// ELOS_Error SYS_file_read(ELOS_File file, uint64_t offset, void* data, uint64_t* size);
// ELOS_Error SYS_file_write(ELOS_File file, uint64_t offset, const void* data, uint64_t* size);
// ELOS_Error SYS_file_info(ELOS_File file, uint64_t* size);

// ELOS_Error SYS_file_remove(const char* path);
// ELOS_Error SYS_file_rename(const char* old_path, const char* new_path);
// ELOS_Error SYS_file_mkdir(const char* path);


/*
    Asynchonrous operations

    @TODO What to support:
        Timer wait, events, signaling?
*/

typedef enum {
    ELOS_ASYNC_KERNEL_POLLING = 0x1,
} ELOS_AsyncCreateFlag;

enum _ELOS_AsyncOperation {
    ELOS_ASYNC_INVALID = 0,

    // File operations
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

    // @TODO File monitor

    // @TODO Network operations

    ELOS_ASYNC_NET_OPEN,
    ELOS_ASYNC_NET_CLOSE,
    ELOS_ASYNC_NET_WRITE,
    ELOS_ASYNC_NET_READ,

    ELOS_ASYNC_DISK_OPEN,
    ELOS_ASYNC_DISK_CLOSE,
    ELOS_ASYNC_DISK_INFO,
    ELOS_ASYNC_DISK_WRITE,
    ELOS_ASYNC_DISK_READ,
    ELOS_ASYNC_DISK_ENUMERATE,

};
typedef uint16_t ELOS_AsyncOperation;


typedef void* ELOS_Net_Handle;

typedef enum {
    ELOS_NET_PROTO_RAW,
    ELOS_NET_PROTO_UDP_IPV4,
    ELOS_NET_PROTO_TCP_IPV4,
    ELOS_NET_PROTO_UDP_IPV6,
    ELOS_NET_PROTO_TCP_IPV6,
} _ELOS_Net_Protocol;
typedef uint8_t ELOS_Net_Protocol;

typedef struct {
    // 0.0.0.0
    // 255.255.255.255:65535
    // ::
    // [0000:0000:0000:0000:0000:0000:0000:0000]:65535
    union {
        // char identifier[64]; // last character is reserved to be NULL
        struct {
            ELOS_Net_Protocol protocol;
            union {
                struct {
                    NET_Device* device;
                } raw;
                struct {
                    uint32_t address;
                    uint16_t port;
                } udp_tcp4;
                struct {
                    uint16_t address[8];
                    uint16_t port;
                } udp_tcp6;
            };
        };
    };
} ELOS_Net_Address;

typedef enum {
    ELOS_FILE_OPEN_FLAG_READ_ONLY = 0x1, // Allows multiple readers on same file.
    ELOS_FILE_OPEN_FLAG_CREATE = 0x2, // Create file if missing
} ELOS_FileOpenFlag;

typedef struct {
    uint64_t  fileSize;
    uint64_t  lastWriteTime_us;
    uint32_t  sectorSize;
    bool isDirectory;
    bool readOnly;
} ELOS_FileInfo;

typedef struct {
    char name[63];
    uint8_t   name_len;
    uint64_t  fileSize;
    uint64_t  lastWriteTime_us;
    bool isDirectory;
    bool isReadOnly;
} ELOS_DirectoryEntry;

typedef uint32_t ELOS_DiskID;
typedef void*    ELOS_DiskHandle;

typedef enum {
    ELOS_DISK_ACCESS_READ      = 0x1,
    ELOS_DISK_ACCESS_WRITE     = 0x2,
    ELOS_DISK_ACCESS_EXCLUSIVE = 0x4,
} ELOS_DiskAccessFlag;

typedef struct {
    char name[63];
    uint8_t   name_len;
    uint64_t  diskSize;
    uint32_t  sectorSize;
} ELOS_DiskInfo;

typedef struct {
    ELOS_DiskID diskID;
    char   name[63];
    uint8_t     name_len;
    uint64_t    diskSize;
    uint32_t    sectorSize;
} ELOS_DiskEntry;


typedef struct {
    uint16_t operation;
    uint16_t flags;
    uint32_t _reserved;
    uint64_t userData;

    union {
        struct {
            const char* path;
            ELOS_PADDING
            ELOS_FileOpenFlag flags;
        } open;
        struct {
            ELOS_File file;
            ELOS_PADDING
        } close;
        struct {
            ELOS_File file;
            ELOS_PADDING
            uint64_t       offset;
            uint64_t       size;
            void*     buffer;
            ELOS_PADDING
        } read;
        struct {
            ELOS_File file;
            ELOS_PADDING
            uint64_t       offset;
            uint64_t       size;
            const void*     buffer;
            ELOS_PADDING
        } write;
        struct {
            ELOS_File      file;
            ELOS_PADDING
            ELOS_FileInfo* fileInfo;
            ELOS_PADDING
        } info;
        struct {
            const char* path;
            ELOS_PADDING
        } remove;
        struct {
            const char* oldPath;
            ELOS_PADDING
            const char* newPath;
            ELOS_PADDING
        } rename;
        struct {
            const char* srcPath;
            ELOS_PADDING
            const char* dstPath;
            ELOS_PADDING
        } copy;
        struct {
            const char* path;
            ELOS_PADDING
        } mkdir;
        struct {
            const char*          path;
            ELOS_PADDING
            uint64_t                  cookie;
            uint32_t                  maxEntries;
            uint32_t                  _reserved;
            ELOS_DirectoryEntry* buffer;
            ELOS_PADDING
        } readdir;

        struct {
            const ELOS_Net_Address* address;
            ELOS_PADDING
        } net_open;
        struct {
            ELOS_Net_Handle handle;
            ELOS_PADDING
        } net_close;
        struct {
            ELOS_Net_Handle         handle;
            ELOS_PADDING
            const ELOS_Net_Address* address;
            ELOS_PADDING
            const void*             data;
            ELOS_PADDING
            uint32_t                     size;
        } net_write;
        struct {
            ELOS_Net_Handle     handle;
            ELOS_PADDING
            ELOS_Net_Address*   address;
            ELOS_PADDING
            void*               buffer;
            ELOS_PADDING
            uint32_t                 bufferSize;
            uint32_t                 _reserved;
            uint64_t                 timeout_ns;
        } net_read;

        struct {
            ELOS_DiskID         id;
            ELOS_DiskAccessFlag flags;
        } disk_open;
        struct {
            ELOS_DiskHandle     handle;
            ELOS_PADDING
        } disk_close;
        struct {
            ELOS_DiskID         id;
            uint32_t                 _reserved;
            ELOS_DiskInfo*      info;
            ELOS_PADDING
        } disk_info;
        struct {
            ELOS_DiskHandle     handle;
            ELOS_PADDING
            uint64_t                 offset;
            uint64_t                 size;
            void*               buffer;
            ELOS_PADDING
        } disk_read;
        struct {
            ELOS_DiskHandle     handle;
            ELOS_PADDING
            uint64_t                 offset;
            uint64_t                 size;
            const void*         buffer;
            ELOS_PADDING
        } disk_write;
        struct {
            uint64_t                 cookie;
            uint32_t                 maxEntries;
            uint32_t                 _reserved;
            ELOS_DiskEntry*     buffer;
            ELOS_PADDING
        } disk_enumerate;
    };
} ELOS_AsyncRequest;

typedef struct {
    uint16_t        operation;
    uint16_t        flags;
    ELOS_Error error;
    uint64_t        userData;
    union {
        struct {
            ELOS_File file;
            ELOS_PADDING
        } open;
        struct {
            uint64_t readBytes;
        } read;
        struct {
            uint64_t writtenBytes;
        } write;
        struct {
            uint64_t cookie;
            uint64_t entryCount;
        } readdir;

        struct {
            ELOS_Net_Handle handle;
            ELOS_PADDING
        } net_open;
        struct {
            uint32_t  readBytes;
        } net_read;


        struct {
            ELOS_DiskHandle handle;
            ELOS_PADDING
        } disk_open;
        struct {
            uint64_t readBytes;
        } disk_read;
        struct {
            uint64_t writtenBytes;
        } disk_write;
        struct {
            uint64_t cookie;
            uint64_t entryCount;
        } disk_enumerate;
    };
} ELOS_AsyncCompletion;

typedef struct {
    volatile uint32_t head;
    volatile uint32_t tail;
    const    uint32_t ringMask;
             uint32_t reserved;
    volatile ELOS_AsyncRequest entries[];
} ELOS_AsyncRequestRing;

typedef struct {
    volatile uint32_t head;
    volatile uint32_t tail;
    const    uint32_t ringMask;
             uint32_t reserved;
    volatile ELOS_AsyncCompletion entries[];
} ELOS_AsyncCompletionRing;


/*
    Creates two rings for asynronous operations.

    @pre ELOS_PERM_ASYNC permission is required.
*/
ELOS_Error SYS_create_async_rings(uint32_t maxEntries, ELOS_AsyncCreateFlag flags, ELOS_AsyncRequestRing** requestRing, ELOS_AsyncCompletionRing** completionRing);


/*
    Destroys two rings. Operations in progress are aborted.

    @pre ELOS_PERM_ASYNC permission is required.
*/
ELOS_Error SYS_destroy_async_rings(ELOS_AsyncRequestRing* requestRing, ELOS_AsyncCompletionRing* completionRing);


/*
    Submits entries in the ring for processing by the kernel. Not necessary if
    ring was created with ELOS_ASYNC_KERNEL_POLLING.

    @pre ELOS_PERM_ASYNC permission is required.
*/
ELOS_Error SYS_submit_async_ring(ELOS_AsyncRequestRing* requestRing);


/*
    Waits for kernel to complete an operation.

    @pre ELOS_PERM_ASYNC permission is required.
*/
ELOS_Error SYS_wait_async_ring(ELOS_AsyncCompletionRing* completionRing, uint64_t timeout_ns);



// @TODO SYS_utc_epoch_time(uint64_t* nanoseconds)
//   Network Time Protocol and DNS to sync the time.


#endif // ELOS_SYSCALL_INCLUDE

// Auto-generated
#include "elos/elos_impl.h"
