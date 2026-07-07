
#include "elos/async.h"

#include "elos/common/string.h"

#include "elos/kernel_console.h"
#include "elos/physical_memory.h"
#include "elos/cpu.h"

#include "elos/vfs.h"


#define printf(...) KCON_printf(__VA_ARGS__)

#define MAX_ASYNC_RINGS 50

typedef struct {
    bool used;
    ELOS_AsyncRequestRing*    requestRing;
    ELOS_AsyncCompletionRing* completionRing;
    u32 ringMask;
} AsyncRing;


volatile u32 g_async_lock;

AsyncRing asyncRings[MAX_ASYNC_RINGS];

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

    *requestRing = ring->requestRing;
    *completionRing = ring->completionRing;

    returnValue = 0;
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

    // @TODO Notify kernel that ring should be processed.

    returnValue = 0;
exit:
    UNLOCK_INT(&g_async_lock);
    return returnValue;
}

int ASYNC_wait_async_ring(ELOS_AsyncCompletionRing* completionRing, u64 timeout_ns) {
    int returnValue = -1;
    LOCK_INT(&g_async_lock);

    AsyncRing* ring = getAsyncRing(NULL, completionRing);
    if (!ring) {
        goto exit;
    }

    // @TODO How do we wait for completion?

    returnValue = 0;
exit:
    UNLOCK_INT(&g_async_lock);
    return returnValue;
}


/*
    Some implemnentation handling functions.
    Called from a kernel thread. It will process a request.
*/


void ASYNC_request_handler(AsyncRing* ring, ELOS_AsyncRequest* request);

u32 availItems(void* _ring, u32 ringMask) {
    ELOS_AsyncRequestRing* ring = _ring;

    u32 head = ring->head & ringMask;
    u32 tail = ring->tail & ringMask;
    if (tail >= head) {
        // [  H----T  ]
        return tail - head;
    } else {
        // [--T   H---]
        return tail + (ringMask+1) - head;
    }
}

u32 freeItems(void* _ring, u32 ringMask) {
    ELOS_AsyncRequestRing* ring = _ring;
    return (ringMask+1) - availItems(ring, ringMask);
}

void ASYNC_handler() {

    for (int ri=0;ri<MAX_ASYNC_RINGS;ri++) {
        AsyncRing* ring = &asyncRings[ri];
        if (!ring->used) {
            continue;
        }

        // @TODO Should be critical section?
        //   A way to reserve completions? (internally)

        if (availItems(ring->requestRing, ring->ringMask) == 0) {
            // No requests
            continue;
        }
        if (freeItems(ring->completionRing, ring->ringMask) == 0) {
            // No free space for completions
            continue;
        }

        u32 head = ring->requestRing->head & ring->ringMask;
        ELOS_AsyncRequest request = ring->requestRing->entries[head];
        head++;

        ASYNC_request_handler(ring, &request);
    }

}

#define VFS_HANDLE_TO_ELOS_FILE(HANDLE) ((ELOS_File)(HANDLE))

void ASYNC_request_handler(AsyncRing* ring, ELOS_AsyncRequest* request) {

    switch (request->operation) {
        case ELOS_ASYNC_FILE_OPEN: {

            VFS_Handle handle = VFS_open(request->open.path, 0);

            u32 tail = ring->completionRing->tail & ring->ringMask;
            volatile ELOS_AsyncCompletion* completion = &ring->completionRing->entries[tail];

            completion->operation = request->operation;
            completion->userData = request->userData;
            completion->flags = 0;
            completion->open.file = NULL;
            if (handle) {
                completion->error = ELOS_OK;
                completion->open.file = VFS_HANDLE_TO_ELOS_FILE(handle);
            } else {
                completion->error = ELOS_GENERIC_ERROR;
            }

            ring->completionRing->tail++;

            // @TODO Notify threads that called 'wait_async_ring'
            
        } break;
        case ELOS_ASYNC_FILE_READ: {
            
            // @TODO What if application frees the buffer while it's being written to?
            u64 result = VFS_read(request->read.file, request->read.offset, request->read.size, request->read.buffer);

            u32 tail = ring->completionRing->tail & ring->ringMask;
            volatile ELOS_AsyncCompletion* completion = &ring->completionRing->entries[tail];

            completion->operation = request->operation;
            completion->userData = request->userData;
            completion->flags = 0;
            completion->read.readBytes = 0;
            if (result) {
                completion->error = ELOS_OK;
                completion->read.readBytes = result;
            } else {
                completion->error = ELOS_GENERIC_ERROR;
            }

            ring->completionRing->tail++;

            // @TODO Notify threads that called 'wait_async_ring'
            
        } break;
        case ELOS_ASYNC_FILE_WRITE: {
            
            // @TODO What if application frees the buffer while it's being read from to?
            u64 result = VFS_write(request->write.file, request->write.offset, request->write.size, request->write.buffer);

            u32 tail = ring->completionRing->tail & ring->ringMask;
            volatile ELOS_AsyncCompletion* completion = &ring->completionRing->entries[tail];

            completion->operation = request->operation;
            completion->userData = request->userData;
            completion->flags = 0;
            completion->write.writtenBytes = 0;
            if (result) {
                completion->error = ELOS_OK;
                completion->write.writtenBytes = result;
            } else {
                completion->error = ELOS_GENERIC_ERROR;
            }

            ring->completionRing->tail++;

            // @TODO Notify threads that called 'wait_async_ring'
            
        } break;
        case ELOS_ASYNC_FILE_CLOSE: {
            
            VFS_close(request->close.file);

            u32 tail = ring->completionRing->tail & ring->ringMask;
            volatile ELOS_AsyncCompletion* completion = &ring->completionRing->entries[tail];

            completion->operation = request->operation;
            completion->userData = request->userData;
            completion->flags = 0;
            completion->error = ELOS_OK;

            ring->completionRing->tail++;

            // @TODO Notify threads that called 'wait_async_ring'
            
        } break;
    }

}
