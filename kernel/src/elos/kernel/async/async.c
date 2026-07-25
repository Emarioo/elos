
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
    ELOS_AsyncRequestRing*    requestRing;
    ELOS_AsyncCompletionRing* completionRing;
    u32 ringMask;
    ELOS_AsyncCreateFlag flags;
    bool used;
    EXEC_Thread* thread; // refer to ID instead?
} AsyncRing;


volatile u32 g_async_lock;

AsyncRing asyncRings[MAX_ASYNC_RINGS];

void ASYNC_handler_specific(AsyncRing* ring);


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

// int ASYNC_wait_async_ring(ELOS_AsyncCompletionRing* completionRing, u64 timeout_ns) {
//     int returnValue = -1;
//     LOCK_INT(&g_async_lock);

//     AsyncRing* ring = getAsyncRing(NULL, completionRing);
//     if (!ring) {
//         goto exit;
//     }

//     // Thread is waiting for an operation to complete.
//     // First check if any is completed.
//     // If not schedule another process.
//     // When it's time to reschedule we check if any IO operation has completed.
//     // pendingIO=false.

//     // @TODO We should suspend the user process and schedule other processes.
//     //   Maybe prioritize or make sure a kernel thread processing async operations is running.

//     returnValue = 0;
// exit:
//     UNLOCK_INT(&g_async_lock);
//     return returnValue;
// }


/*
    Some implemnentation handling functions.
    Called from a kernel thread. It will process a request.
*/


void ASYNC_request_handler(AsyncRing* ring, ELOS_AsyncRequest* request);

// u32 availItems(void* _ring, u32 ringMask) {
//     ELOS_AsyncRequestRing* ring = _ring;

//     u32 head = ring->head & ringMask;
//     u32 tail = ring->tail & ringMask;
//     if (tail >= head) {
//         // [  H----T  ]
//         return tail - head;
//     } else {
//         // [--T   H---]
//         return tail + (ringMask+1) - head;
//     }
// }

// u32 freeItems(void* _ring, u32 ringMask) {
//     ELOS_AsyncRequestRing* ring = _ring;
//     return (ringMask+1) - availItems(ring, ringMask);
// }


void ASYNC_handler() {

    for (int ri=0;ri<MAX_ASYNC_RINGS;ri++) {
        AsyncRing* ring = &asyncRings[ri];
        if (!ring->used && 0 == (ring->flags & ELOS_ASYNC_KERNEL_POLLING)) {
            continue;
        }

        ASYNC_handler_specific(ring);
    }

}


void ASYNC_handler_specific(AsyncRing* ring) {
    // These checks assumes one completion producer
    if (ring->requestRing->head == ring->requestRing->tail) {
        // No requests
        return;
    }
    if (ring->completionRing->head - ring->completionRing->tail >= ring->ringMask) {
        // No free space for completions
        return;
    }

    u32 tail = ring->requestRing->tail & ring->ringMask;
    ELOS_AsyncRequest request = ring->requestRing->entries[tail];
    // Store request on stack so another one can be added by user programs.
    // We cannot increment tail until we have "popped" and used the entry for it
    // may get overriden otherwise.
    __atomic_fetch_add(&ring->requestRing->tail, 1, __ATOMIC_SEQ_CST);

    ASYNC_request_handler(ring, &request);
}

#define VFS_HANDLE_TO_ELOS_FILE(HANDLE) ((ELOS_File)(HANDLE))
#define ELOS_FILE_TO_VFS_HANDLE(HANDLE) ((ELOS_File)(HANDLE))


bool map_user_buffer(PageTable* userTable, const void* buffer, size_t size) {
    PageTable* kernelPageTable = (void*)read_cr3();

    uintptr_t virtAddress = (uintptr_t)buffer & ~(uintptr_t)(PAGE_SIZE-1);
    uintptr_t virtAddressEnd = ((uintptr_t)buffer + size + PAGE_SIZE-1) & ~(uintptr_t)(PAGE_SIZE-1);
    while (virtAddress < virtAddressEnd) {
        uintptr_t phys = (uintptr_t)PMEM_virt_to_phys(userTable, (void*)virtAddress);
        if (phys == 0)
            return false;
        bool mapped = PMEM_map_memory(kernelPageTable, (void*)virtAddress, (void*)phys, PAGE_SIZE, PMEM_FLAG_NONE);
        if (!mapped) {
            printf("map_user_buffer: Could not map 0x%x -> 0x%x\n", virtAddress, phys);
            return false;
        }
        virtAddress += PAGE_SIZE;
    }
    return true;
}

void ASYNC_request_handler(AsyncRing* ring, ELOS_AsyncRequest* request) {
    volatile ELOS_AsyncCompletion* completion;

    // We assume one producer on the kernel side.
    u32 head = ring->completionRing->head & ring->ringMask;
    completion = &ring->completionRing->entries[head];
    completion->flags = 0;

    // @TODO Make some macros for redundant stuff so when we need to refactor
    //   it's less manual work.

    #define ASYNC_COMPLETE() \
        completion->operation = request->operation;   \
        completion->userData = request->userData;     \
        completion->flags = 0;                        \
        __atomic_thread_fence(__ATOMIC_ACQUIRE);      \
        __atomic_fetch_add(&ring->completionRing->head, 1, __ATOMIC_SEQ_CST); \
        __atomic_thread_fence(__ATOMIC_RELEASE);      \
        ring->thread->waitingForIO = false;


    switch (request->operation) {
        case ELOS_ASYNC_FILE_OPEN: {

            VFS_Handle handle = VFS_open(request->open.path, 0);

            completion->open.file = NULL;
            if (handle) {
                completion->error = ELOS_OK;
                completion->open.file = VFS_HANDLE_TO_ELOS_FILE(handle);
            } else {
                completion->error = ELOS_GENERIC_ERROR;
            }

            ASYNC_COMPLETE();

            // @TODO Notify threads that called 'wait_async_ring'
            
        } break;
        case ELOS_ASYNC_FILE_READ: {

            // @TODO Check that buffer is a valid address in user address space.
            //    The address itself is not enough, all pages accessed by [buffer:size]
            //    needs to be checked.

            // @TODO Next we need to write file data into the buffer.
            //   We can't simply take virtual to physical address and pass
            //   that because the whole buffer may not access pages contiguously.

            //   We can map the buffer into kernel space.
            //   Utilizing paging i

            bool mapped = map_user_buffer((void*)ring->thread->frame.cr3, request->read.buffer, request->read.size);
            if (!mapped) {
                completion->error = ELOS_GENERIC_ERROR;
            } else {
                // @TODO What if application frees the buffer while it's being written to?
                u64 result = VFS_read(ELOS_FILE_TO_VFS_HANDLE(request->read.file), request->read.offset, request->read.size, request->read.buffer);

                completion->read.readBytes = 0;
                if (result == request->read.size) {
                    completion->error = ELOS_OK;
                    completion->read.readBytes = result;
                } else {
                    completion->error = ELOS_GENERIC_ERROR;
                }
            }

            ASYNC_COMPLETE();

            // @TODO Notify threads that called 'wait_async_ring'
            
        } break;
        case ELOS_ASYNC_FILE_WRITE: {
            
            // @TODO What if application frees the buffer while it's being read from to?

            bool mapped = map_user_buffer((void*)ring->thread->frame.cr3, request->write.buffer, request->write.size);

            if (!mapped) {
                completion->error = ELOS_GENERIC_ERROR;
            } else {
                u64 result = VFS_write(ELOS_FILE_TO_VFS_HANDLE(request->write.file),
                    request->write.offset, request->write.size, request->write.buffer);

                completion->write.writtenBytes = 0;
                if (result == request->write.size) {
                    completion->error = ELOS_OK;
                    completion->write.writtenBytes = result;
                } else {
                    completion->error = ELOS_GENERIC_ERROR;
                }
            }
            
            ASYNC_COMPLETE();

            // @TODO Notify threads that called 'wait_async_ring'
            
        } break;
        case ELOS_ASYNC_FILE_CLOSE: {
            
            VFS_close(request->close.file);

            completion->flags = 0;
            completion->error = ELOS_OK;

            ASYNC_COMPLETE();

            // @TODO Notify threads that called 'wait_async_ring'
            
        } break;
        case ELOS_ASYNC_FILE_INFO: {
            VFS_HandleInfo info;

            bool mapped = map_user_buffer((void*)ring->thread->frame.cr3, request->info.fileInfo, sizeof(*request->info.fileInfo));

            if (!mapped) {
                completion->error = ELOS_GENERIC_ERROR;
            } else {

                // @TODO Handle errors!?
                VFS_info(ELOS_FILE_TO_VFS_HANDLE(request->info.file), &info);

                // The info struct in request must be mapped.
                // How do we ensure that here?
                request->info.fileInfo->fileSize          = info.fileSize;
                request->info.fileInfo->blockSize         = info.blockSize;
                request->info.fileInfo->isDirectory       = info.isDirectory;
                request->info.fileInfo->readOnly          = info.readOnly;
                request->info.fileInfo->lastWriteTime_us  = info.lastWriteTime_us;

                completion->flags = 0;
                completion->error = ELOS_OK;
            }

            ASYNC_COMPLETE();

            // @TODO Notify threads that called 'wait_async_ring'
            
        } break;
        default: {
            printf("ASYNC_request_handler: Unhandled operation %d (0 is invalid)\n", request->operation);
        } break;
    }

}
