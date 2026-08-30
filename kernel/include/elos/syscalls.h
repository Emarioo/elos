/*
    This files describes the available syscalls in ELOS.

    User processes call syscalls. The syscall operates based on the
    process's capabilities.

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

// consider using stdint.h instead
#include "elos/common/types.h"
#include "elos/keycode.h"

#if defined(__x86_64__)
#define ELOS_PADDING
#else
#define ELOS_LINETHING(X,Y) X##Y
#define ELOS_LINETHING2(Y) ELOS_LINETHING(_reserved, Y)
#define ELOS_PADDING u32 ELOS_LINETHING2(__LINE__);
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

    ELOS_ERR_INVALID_PARAM,
    // Syscall number was not known by the OS. Also used in AsyncCompletion.error if operation was invalid.
    ELOS_ERR_INVALID_SYSCALL,
    ELOS_ERR_CAP_DENIED,

    ELOS_ERR_NOT_FOUND,
    ELOS_ERR_BUSY,


    // ELOS_ERR_IPC_FULL,

    ELOS_ERR_UNSUPPORTED_AUDIO_FORMAT,
    ELOS_ERR_BUFFER_SIZE_NOT_FRAME_ALIGNED,
    ELOS_ERR_BUFFER_SIZE_TOO_BIG,
    ELOS_ERR_BUFFER_SIZE_TOO_SMALL,
    ELOS_ERR_NO_AUDIO_DEVICE,
    
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
    ELOS_CAP_AUDIO           = (1<<12),
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
    ELOS_PADDING
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
    Returns error in string form.
*/
const char* elos_error(ELOS_Error err);


/*
    Returns capabilities of process.

    @param capabilities Filled with information.
*/
void SYS_capabilities(ELOS_Capabilities* capabilities);


/*
    Asks the OS for capabilities. Check the in parameter which capabilities
    where accepted. Once they have been accepted you can safely use them.
    Once a capability has been accepted you can never lose it.

    @param capabilities Zero all but the capabilities you want to request.
    On return the accepted capabilities remain non-zero and denied ones become zero.
*/
void SYS_request_capabilities(ELOS_Capabilities* capabilities);


/*
    Sends text to OS. It may print it to serial out, to frame buffer or do nothing.

    @pre No capability required.
*/
void SYS_debug_log(const char* text, u32 length);


/*
    Allocates memory from the heap.

    @pre ELOS_CAP_HEAP capability is required.
*/
ELOS_Error SYS_heap_allocate(void** newAddress, size_t size);
ELOS_Error SYS_heap_free(void* oldAddress);
ELOS_Error SYS_heap_reallocate(void** newAddress, size_t size, void* oldAddress);
ELOS_Error SYS_heap_map(void* virtAddress, size_t size);


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
ELOS_Error SYS_service_create(const char* name, ELOS_ServiceEndpoint* endpoint, u32 queueSize);


/*
    Creates a connection to a service. It allows you to send and receive messages
    between processes.

    @pre ELOS_CAP_SERVICE_CLIENT capability is required.
*/
ELOS_Error SYS_service_connect(const char* name, ELOS_ServiceEndpoint* endpoint, u32 queueSize);


/*
    Sends messages to the service channel.

    @pre ELOS_CAP_SERVICE_SERVER or ELOS_CAP_SERVICE_CLIENT capability is required.

    @param endpoint The endpoint to send from.
    @param senderEndpoint Only relevant if endpoint is a servie and not the connection to the service.
    @param data Buffer to send.
    @param size Size of buffer to send.
    @return ELOS_IPC_FULL if service channel is full. ELOS_INVALID_PARAM if endpoint or data pointer are invalid.
*/
ELOS_Error SYS_service_send(ELOS_ServiceEndpoint endpoint, const void* data, u32 size);


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
ELOS_Error SYS_service_recv(ELOS_ServiceEndpoint endpoint, ELOS_ServiceEndpoint* senderEndpoint, const void** data, u32* size, u64 timeout_ns);


/*
    Allocate memory that can be shared with other processes.

    @pre ELOS_CAP_SHARED_MEMORY capability is required.
*/
ELOS_Error SYS_shared_memory_create(size_t size, ELOS_SharedMemory* handle);


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
ELOS_Error SYS_shared_memory_info(ELOS_SharedMemory handle, void** buffer, size_t* size);


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
// void SYS_thread_exit();

/*
    Spawn a thread.

    @param entry  Entry point of the thread.
    @param handle Handle to the thread
*/
// ELOS_Error SYS_spawn_thread(FN_thread_entry entry, ELOS_ThreadHandle* handle);

/*
    Join a thread into the thread that spawned it. 

    @param handle Handle to the thread
*/
// ELOS_Error SYS_join(ELOS_ThreadHandle handle);

/*
    @return ID of current thread.
*/
// ELOS_ThreadID SYS_self();

/*
    Spawn a process.

    @TODO Should we get handle to it so we can wait for it to finish.
          We want to explore unconvential ways to do capabilities/threads/processes.
          Domain execution is the term I use to differentiate from normal processes.

    @param path     Path to the executable.
    @param data     Data to pass to the executable. Process should parse flags from it. ()
    @param data_len Size of the data.
*/
// ELOS_Error SYS_spawn_process(const char* path, const char* data, u32 data_len);

// @TODO Spawn a process.



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
    u32 sampleRate;
    u8  channels;
    ELOS_AudioSampleFormat sampleFormat;
} ELOS_AudioFormat;

typedef struct {
    char name[32];
} ELOS_AudioDeviceInfo;

typedef struct {
    u32 head;
    u32 tail;
    u32 size;
    u8  data[];
} ELOS_AudioBuffer;

/*
    Returns the default audio device.

    @param device The returned default device.

    @pre ELOS_CAP_AUDIO is required.

    @exception ELOS_NO_AUDIO_DEVICE No audio device.
*/
ELOS_Error SYS_default_audio(ELOS_AudioDevice* device);

// @TODO Provide audio device enumeration functions.

/*
    Returns information about the audio device such as name and audio format.

    @param device The device to get information from.
    @param info The information of the device.

    @pre ELOS_CAP_AUDIO is required.

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

    @pre ELOS_CAP_AUDIO is required.

    @exception ELOS_UNSUPPORTED_AUDIO_FORMAT ELOS_BUSY
    
*/
ELOS_Error SYS_create_audio_buffer(ELOS_AudioDevice device, ELOS_AudioFormat* format, u32 bufferSize, ELOS_AudioBuffer** buffer);

/*
    Destroys and frees audio buffer.

    @param device The device the buffer is from.
    @param buffer The buffer to destroy.

    @pre ELOS_CAP_AUDIO is required.

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
// ELOS_Error SYS_file_read(ELOS_File file, u64 offset, void* data, u64* size);
// ELOS_Error SYS_file_write(ELOS_File file, u64 offset, const void* data, u64* size);
// ELOS_Error SYS_file_info(ELOS_File file, u64* size);

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

    // 
};
typedef u16 ELOS_AsyncOperation;

typedef enum {
    ELOS_FILE_OPEN_FLAG_READ_ONLY = 0x1, // Allows multiple readers on same file.
    ELOS_FILE_OPEN_FLAG_CREATE = 0x2, // Create file if missing
} ELOS_FileOpenFlag;

typedef struct {
    u64  fileSize;
    u64  lastWriteTime_us;
    u32  blockSize;
    bool isDirectory;
    bool readOnly;
} ELOS_FileInfo;

typedef struct {
    char name[63];
    u8   name_len;
    u64  fileSize;
    u64  lastWriteTime_us;
    bool isDirectory;
    bool isReadOnly;
} ELOS_DirectoryEntry;

typedef struct {
    ELOS_AsyncOperation operation;
    u16 flags;
    u32 _reserved;
    u64 userData;

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
            u64       offset;
            u64       size;
            void*     buffer;
            ELOS_PADDING
        } read;
        struct {
            ELOS_File file;
            ELOS_PADDING
            u64       offset;
            u64       size;
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
            u64                  cookie;
            u64                  maxEntries;
            ELOS_DirectoryEntry* buffer;
            ELOS_PADDING
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
            ELOS_PADDING
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
    volatile u32 head;
    volatile u32 tail;
    const    u32 ringMask;
             u32 reserved;
    volatile ELOS_AsyncRequest entries[];
} ELOS_AsyncRequestRing;

typedef struct {
    volatile u32 head;
    volatile u32 tail;
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


#endif // ELOS_SYSCALL_INCLUDE

// Auto-generated
#include "syscalls_impl.h"
