
#include "elos/async.h"

#include "elos/common/string.h"
#include "elos/common/intrinsics.h"

#include "elos/kernel_console.h"
#include "elos/physical_memory.h"
#include "elos/cpu.h"

#include "elos/execution.h"

#include "elos/vfs.h"


#define printf(...) KCON_printf(__VA_ARGS__)

#define MAX_ASYNC_RINGS 50

typedef struct {
    // @TODO tail and head in request and completion must be stored separately and copied into
    //   the rings. Otherwise user space can modify them and mess things up.
    ELOS_AsyncRequestRing*    requestRing;
    ELOS_AsyncCompletionRing* completionRing;
    u32 ringMask;
    ELOS_AsyncCreateFlag flags;
    bool used;
    EXEC_Thread* thread; // refer to ID instead?
} AsyncRing;


volatile u32 g_async_lock;

AsyncRing asyncRings[MAX_ASYNC_RINGS];

bool ASYNC_handler_specific(AsyncRing* ring);
void ASYNC_request_handler(AsyncRing* ring, ELOS_AsyncRequest* request);


u32 ringMaskFromEntryCount(u32 count) {
    if (count == 1)
        return 1;

    u32 bit = 31;
    while (bit >= 0) {
        u32 shifted_bit = 1 << bit;
        if (shifted_bit & count)  {
            if (count-1 && (shifted_bit & count) == 0) {
                return count-1;
            } else if(bit == 31) {
                return -1;
            } else {
                return (1 << (bit + 1)) - 1;
            }
        }
        bit--;
    }
    return 0;
}


AsyncRing* makeAsyncRing(u32 ringMask) {
    AsyncRing* newRing = NULL;
    for (int i=0;i<ARRAY_LENGTH(asyncRings);i++) {
        AsyncRing* ring = &asyncRings[i];
        if (!ring->used) {
            newRing = ring;
            break;
        }
    }
    if (!newRing) {
        return NULL;
    }
    
    u32 maxEntries = ringMask + 1;

    u64 requestRingSize = sizeof(ELOS_AsyncRequestRing) + maxEntries * sizeof(ELOS_AsyncRequest);
    u64 completetionRingSize = sizeof(ELOS_AsyncCompletionRing) + maxEntries * sizeof(ELOS_AsyncCompletion);

    void* requestAddress = PMEM_alloc_phys(requestRingSize, PMEM_FLAG_IDENTITY_MAPPED);
    if (!requestAddress) {
        return NULL;
    }
    void* completeAddress = PMEM_alloc_phys(completetionRingSize, PMEM_FLAG_IDENTITY_MAPPED);
    if (!completeAddress) {
        // @TODO @MEMORY_LEAK Free requestAddress!
        return NULL;
    }

    memset(requestAddress, 0, requestRingSize);
    memset(completeAddress, 0, completetionRingSize);

    newRing->ringMask = ringMask;

    newRing->requestRing = requestAddress;
    *(u32*)&newRing->requestRing->ringMask = ringMask;

    newRing->completionRing = completeAddress;
    *(u32*)&newRing->completionRing->ringMask = ringMask;

    newRing->used = true;
    return newRing;
}

AsyncRing* getAsyncRing(ELOS_AsyncRequestRing* requestRing, ELOS_AsyncCompletionRing* completionRing) {
    AsyncRing* foundRing = NULL;
    for (int i=0;i<ARRAY_LENGTH(asyncRings);i++) {
        AsyncRing* ring = &asyncRings[i];
        if (ring->used && (ring->requestRing == requestRing || ring->completionRing == completionRing)) {
            foundRing = ring;
            break;
        }
    }
    if (!foundRing) {
        return NULL;
    }

    return foundRing;
}

int ASYNC_create_async_rings(u32 maxEntries, ELOS_AsyncCreateFlag flags, ELOS_AsyncRequestRing** requestRing, ELOS_AsyncCompletionRing** completionRing) {
    int returnValue = -1;
    if (flags & ELOS_ASYNC_KERNEL_POLLING) {
        printf("ASYNC OPERATION KERNEL POLLING NOT SUPPORTED\n");
        return returnValue;
    }
    if (maxEntries > 0x10000) {
        printf("ASYNC RING max 65536 entries\n");
        return returnValue;
    }

    LOCK_INT(&g_async_lock);

    // @TODO Does process have enough memory for maxEntries?

    u32 ringMask = ringMaskFromEntryCount(maxEntries);

    AsyncRing* ring = makeAsyncRing(ringMask);
    if (!ring) {
        goto exit;
    }
    
    ring->flags = flags;

    int coreIndex = CPU_get_core_index();
    EXEC_Core* core = &cores[coreIndex];
    EXEC_Thread* activeThread = &core->threads[core->active_thread];
    ring->thread = activeThread;

    *requestRing = ring->requestRing;
    *completionRing = ring->completionRing;

    returnValue = ASYNC_OK;
exit:
    UNLOCK_INT(&g_async_lock);
    return returnValue;
}

int ASYNC_destroy_async_rings(ELOS_AsyncRequestRing* requestRing, ELOS_AsyncCompletionRing* completionRing, u32* maxEntries) {
    int returnValue = -1;
    LOCK_INT(&g_async_lock);

    AsyncRing* ring = getAsyncRing(requestRing, completionRing);
    if (!ring) {
        goto exit;
    }

    if (maxEntries) {
        *maxEntries = ring->ringMask+1;
    }

    PMEM_free(ring->requestRing);
    PMEM_free(ring->completionRing);
    ring->used = false;

    returnValue = 0;
exit:
    UNLOCK_INT(&g_async_lock);
    return returnValue;
}

int ASYNC_submit_async_ring(ELOS_AsyncRequestRing* requestRing) {
    int returnValue = -1;
    LOCK_INT(&g_async_lock);

    AsyncRing* ring = getAsyncRing(requestRing, NULL);
    if (!ring) {
        goto exit;
    }

    // If you want to process synchronously which we don't.
    // @TODO We should not do operations here.
    // while (ring->requestRing->head != ring->requestRing->tail) {
    //     ASYNC_handler_specific(ring);
    // }

    // @TODO Notify kernel that ring should be processed.
    //    

    returnValue = 0;
exit:
    UNLOCK_INT(&g_async_lock);
    return returnValue;
}



void ASYNC_main() {
    while (1) {
        bool anyWork = false;
        for (int ri=0;ri<MAX_ASYNC_RINGS;ri++) {
            AsyncRing* ring = &asyncRings[ri];
            if (!ring->used && 0 == (ring->flags & ELOS_ASYNC_KERNEL_POLLING)) {
                continue;
            }

            bool didWork = ASYNC_handler_specific(ring);
            if (didWork) {
                anyWork = true;
            }
        }

        if (!anyWork) {
            EXEC_sleep(100*1000);
        }
    }
}


bool ASYNC_handler_specific(AsyncRing* ring) {
    // These checks assumes one completion producer
    if (ring->requestRing->head == ring->requestRing->tail) {
        // No requests
        return false;
    }
    if (ring->completionRing->head - ring->completionRing->tail >= ring->ringMask) {
        // No free space for completions
        return false;
    }

    u32 tail = ring->requestRing->tail & ring->ringMask;
    ELOS_AsyncRequest request = ring->requestRing->entries[tail];
    // Store request on stack so another one can be added by user programs.
    // We cannot increment tail until we have "popped" and used the entry for it
    // may get overriden otherwise.
    __atomic_fetch_add(&ring->requestRing->tail, 1, __ATOMIC_SEQ_CST);

    ASYNC_request_handler(ring, &request);
    return true;
}

#define VFS_HANDLE_TO_ELOS_FILE(HANDLE) ((ELOS_File)(HANDLE))
#define ELOS_FILE_TO_VFS_HANDLE(HANDLE) ((ELOS_File)(HANDLE))


bool map_user_buffer(PageTable* userTable, const void* buffer, size_t size) {
    PageTable* kernelPageTable = (void*)read_cr3();

    uintptr_t virtAddress = (uintptr_t)buffer & ~(uintptr_t)(PAGE_SIZE-1);
    uintptr_t virtAddressEnd = ((uintptr_t)buffer + size + PAGE_SIZE-1) & ~(uintptr_t)(PAGE_SIZE-1);
    while (virtAddress < virtAddressEnd) {
        uintptr_t phys = (uintptr_t)PMEM_virt_to_phys(userTable, (void*)virtAddress);
        if (phys == 0) {
            printf("map_user_buffer: Not mapped by user 0x%zx\n", virtAddress);
            return false;
        }
        bool mapped = PMEM_map_memory(kernelPageTable, (void*)virtAddress, (void*)phys, PAGE_SIZE, PMEM_FLAG_NONE);
        if (!mapped) {
            printf("map_user_buffer: Could not map 0x%zx -> 0x%zx\n", virtAddress, phys);
            return false;
        }
        virtAddress += PAGE_SIZE;
    }
    return true;
}

bool map_user_path(PageTable* userTable, const char* path) {
    PageTable* kernelPageTable = (void*)read_cr3();

    const char* pathPointer = path;
    while (1) {
        // Map first page
        uintptr_t phys = (uintptr_t)PMEM_virt_to_phys(userTable, (void*)pathPointer);
        if (phys == 0) {
            printf("map_user_path: Null terminator is not mapped 0x%zx\n", pathPointer);
            break;
        }
        bool mapped = PMEM_map_memory(kernelPageTable, (void*)pathPointer, (void*)phys, PAGE_SIZE, PMEM_FLAG_NONE);
        if (!mapped) {
            printf("map_user_path: Could not map 0x%zx -> 0x%zx\n", pathPointer, phys);
            break;
        }

        // Find null terminator
        const char* pathPointerEnd = (const char*)(((uintptr_t)pathPointer + PAGE_SIZE) & ~(uintptr_t)(PAGE_SIZE-1));
        while (pathPointer != pathPointerEnd) {
            if (*pathPointer == 0) {
                return true;
            }
            pathPointer++;
        }

        // Could not find terminator try next page.
    }
    return false;
}

void ASYNC_request_handler(AsyncRing* ring, ELOS_AsyncRequest* request) {
    ELOS_AsyncCompletion  completion = {0};
    completion.operation = request->operation;
    completion.userData = request->userData;
    completion.error = ELOS_ERR_UNKNOWN;
    completion.flags = 0;

    /* @TODO We need to sanitise and verify all buffers and parameters are valid.
            We assume they are at the moment which isn't good.
            We need to lock buffers to make sure they stay valid too.
            This includes if a process segfaults. The memory must stay valid
            temporarily for the async request handler to finish.
            Or we abort the async handler but we may leave VFS in a bad state
            so I would rather finish and do unncecessary work where buffer request by
            now dead process is deleted when async handler is done at some point.

            Check that buffer is a valid address in user address space.
            The address itself is not enough, all pages accessed by [buffer:size]
            needs to be checked.

            Next we need to write file data into the buffer.
            We can't simply take virtual to physical address and pass
            that because the whole buffer may not access pages contiguously.
    */
    bool _temp_mapped;

    const char* safePath;
    void*       safeBuffer;
    const void* safeConstBuffer;
    size_t      safeBufferSize;
    VFS_Handle  safeVFSHandle;

    #define GET_SANITIZED_PATH(out_PATH, PATH) \
        *out_PATH = PATH; \
        _temp_mapped = map_user_path((void*)ring->thread->frame.cr3, PATH); \
        if (!_temp_mapped)  break;

    #define GET_SANITIZED_FILE(out_FILE, FILE) \
        *out_FILE = VFS_HANDLE_TO_ELOS_FILE(FILE);

    #define GET_SANITIZED_BUFFER(out_BUFFER, out_SIZE, BUFFER, SIZE) \
        *out_BUFFER = BUFFER; \
        *out_SIZE = SIZE; \
        _temp_mapped = map_user_buffer((void*)ring->thread->frame.cr3, BUFFER, SIZE); \
        if (!_temp_mapped)  break;

    #define GET_SANITIZED_STRUCT(out_STRUCT_PTR, STRUCT_PTR) \
        *(void**)out_STRUCT_PTR = STRUCT_PTR; \
        _temp_mapped = map_user_buffer((void*)ring->thread->frame.cr3, STRUCT_PTR, sizeof(*STRUCT_PTR)); \
        if (!_temp_mapped)  break;

    switch (request->operation) {
        case ELOS_ASYNC_FILE_OPEN: {
            GET_SANITIZED_PATH(&safePath, request->open.path);
            
            VFS_Handle handle = VFS_open(safePath, request->open.flags);
            if (!handle) {
                break;
            }

            completion.open.file = VFS_HANDLE_TO_ELOS_FILE(handle);
            completion.error = ELOS_OK;
        } break;
        case ELOS_ASYNC_FILE_READ: {

            GET_SANITIZED_BUFFER(&safeBuffer, &safeBufferSize, request->read.buffer, request->read.size);
            GET_SANITIZED_FILE(&safeVFSHandle, request->read.file);

            u64 result = VFS_read(safeVFSHandle, request->read.offset, safeBufferSize, safeBuffer);

            if (result != safeBufferSize) {
                break;
            }

            completion.error = ELOS_OK;
            completion.read.readBytes = result;
        } break;
        case ELOS_ASYNC_FILE_WRITE: {
            
            GET_SANITIZED_BUFFER(&safeConstBuffer, &safeBufferSize, request->write.buffer, request->write.size);
            GET_SANITIZED_FILE(&safeVFSHandle, request->write.file);
            
            u64 result = VFS_write(safeVFSHandle, request->write.offset, safeBufferSize, safeConstBuffer);

            if (result != request->write.size) {
                break;
            }

            completion.error = ELOS_OK;
            completion.write.writtenBytes = result;
        } break;
        case ELOS_ASYNC_FILE_CLOSE: {
            GET_SANITIZED_FILE(&safeVFSHandle, request->write.file);
            
            VFS_close(safeVFSHandle);

            completion.error = ELOS_OK;
        } break;
        case ELOS_ASYNC_FILE_INFO: {
            VFS_HandleInfo info;
            VFS_HandleInfo* safeVFSHandleInfo;

            GET_SANITIZED_FILE(&safeVFSHandle, request->write.file);
            GET_SANITIZED_STRUCT(&safeVFSHandleInfo, request->info.fileInfo);

            // @TODO Handle errors!?
            bool yes = VFS_info(safeVFSHandle, &info);
            if (!yes) {
                break;
            }

            safeVFSHandleInfo->fileSize          = info.fileSize;
            safeVFSHandleInfo->blockSize         = info.blockSize;
            safeVFSHandleInfo->isDirectory       = info.isDirectory;
            safeVFSHandleInfo->readOnly          = info.readOnly;
            safeVFSHandleInfo->lastWriteTime_us  = info.lastWriteTime_us;
            
            completion.error = ELOS_OK;
            
        } break;
        case ELOS_ASYNC_FILE_REMOVE: {
            GET_SANITIZED_PATH(&safePath, request->remove.path);

            bool yes = VFS_remove(safePath);
            if (!yes) {
                break;
            }

            completion.error = ELOS_OK;
        } break;
        case ELOS_ASYNC_FILE_RENAME: {
            const char* newPath;
            GET_SANITIZED_PATH(&safePath, request->rename.oldPath);
            GET_SANITIZED_PATH(&newPath, request->rename.newPath);

            bool yes = VFS_rename(safePath, newPath);
            if (!yes) {
                break;
            }

            completion.error = ELOS_OK;
        } break;
        case ELOS_ASYNC_FILE_COPY: {
            const char* dstPath;
            GET_SANITIZED_PATH(&safePath, request->copy.srcPath);
            GET_SANITIZED_PATH(&dstPath, request->copy.dstPath);

            bool yes = VFS_copy(safePath, dstPath);
            if (!yes) {
                break;
            }

            completion.error = ELOS_OK;
        } break;
        case ELOS_ASYNC_FILE_MKDIR: {
            GET_SANITIZED_PATH(&safePath, request->mkdir.path);

            bool yes = VFS_mkdir(safePath);
            if (!yes) {
                break;
            }

            completion.error = ELOS_OK;
        } break;
        case ELOS_ASYNC_FILE_READDIR: {
            GET_SANITIZED_PATH(&safePath, request->readdir.path);
            GET_SANITIZED_BUFFER(&safeBuffer, &safeBufferSize, request->readdir.buffer, request->readdir.maxEntries * sizeof(*request->readdir.buffer));

            u64 cookie = request->readdir.cookie;
            u64 entryCount = request->readdir.maxEntries;
            bool yes = VFS_readdir(safePath, &cookie, &entryCount, safeBuffer);
            completion.readdir.cookie = cookie;
            completion.readdir.entryCount = entryCount;

            if (!yes) {
                break;
            }
            completion.error = ELOS_OK;
        } break;
        default: {
            printf("ASYNC_request_handler: Unhandled operation %d (0 is invalid)\n", request->operation);
            completion.error = ELOS_ERR_INVALID_SYSCALL;
        } break;
    }

    // We assume one producer on the kernel side.
    u32 head = ring->completionRing->head & ring->ringMask;
    ring->completionRing->entries[head] = completion;
    __atomic_thread_fence(__ATOMIC_ACQUIRE);
    __atomic_fetch_add(&ring->completionRing->head, 1, __ATOMIC_SEQ_CST);
    __atomic_thread_fence(__ATOMIC_RELEASE);
    // @TODO Notify all threads that called 'wait_async_ring'.
    //    We only have one thread per process at the moment.
    //    We don't have processes yet.
    ring->thread->waitingForIO = false;
}
