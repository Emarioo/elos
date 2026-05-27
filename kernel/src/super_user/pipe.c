
#include "super_user/pipe.h"


#include "elos/physical_memory.h"
#include "elos/cpu.h"

#include "elos/common/string.h"
#include "elos/common/intrinsics.h"

#define malloc(SIZE) PMEM_alloc(SIZE)
#define free(PTR) PMEM_free(PTR)


int create_ringBuffer(uint32_t size, Handle* out_handle) {
    void* buffer = malloc(size);
    if (!buffer)
        return -1;

    HandleData* handle = malloc(sizeof(HandleData));
    if (!handle) {
        free(buffer);
        return -1;
    }

    memset(handle, 0, sizeof(HandleData));
    handle->ring_buffer = buffer;
    handle->ring_buffer_size = size;
    handle->status = PIPE_STATUS_ENABLED;

    *out_handle = (Handle)handle;
    return 0;
}

void ring_write(void* ring_buffer, uint64_t ring_size, uint64_t _head, void* src, uint64_t size) {
    uint64_t head = _head % ring_size;
    if (size + head > ring_size) {
        memcpy((char*)ring_buffer + head, (char*)src, ring_size - head);
        memcpy((char*)ring_buffer, (char*)src + ring_size - head, size - (ring_size - head));
    } else {
        memcpy((char*)ring_buffer + head, (char*)src, size);
    }
}

void ring_read(void* ring_buffer, uint64_t ring_size, uint64_t _tail, void* dst, uint64_t size) {
    uint64_t tail = _tail % ring_size;
    if (size + tail > ring_size) {
        memcpy((char*)dst, (char*)ring_buffer + tail, ring_size - tail);
        memcpy((char*)dst + ring_size - tail, (char*)ring_buffer, size - (ring_size - tail));
    } else {
        memcpy((char*)dst, (char*)ring_buffer + tail, size);
    }
}

int write(Handle _handle, const void* buffer, uint64_t size) {
    HandleData* handle = (HandleData*)_handle;

    uint64_t remainingBytes = size;

    while (remainingBytes) {
        if((handle->status & PIPE_STATUS_ENABLED) == 0)
            goto exit;

        int64_t suspected_freeSpace = handle->ring_buffer_size + handle->tail - handle->head;
        int64_t bytesToWrite = remainingBytes;
        if (suspected_freeSpace < remainingBytes) {
            bytesToWrite = suspected_freeSpace;
        }

        if (bytesToWrite <= 0) {
            pause();
            continue;
        }

        LOCK(&handle->lock);
        
        suspected_freeSpace = handle->ring_buffer_size + handle->tail - handle->head;
        bytesToWrite = remainingBytes;
        if (suspected_freeSpace < remainingBytes) {
            bytesToWrite = suspected_freeSpace;
        }
        if (bytesToWrite <= 0) {
            UNLOCK(&handle->lock);
            continue;
        }

        uint64_t old_head = handle->head;
        handle->head += bytesToWrite;

        ring_write(handle->ring_buffer, handle->ring_buffer_size, old_head, (char*)buffer + size - remainingBytes, bytesToWrite);

        UNLOCK(&handle->lock);

        remainingBytes -= bytesToWrite;
    }

exit:
    return size - remainingBytes;
}

int read(Handle _handle, void* buffer, uint64_t size) {
    HandleData* handle = (HandleData*)_handle;

    // @TODO When looping check status flag. We may need sleeping.
    uint64_t remainingBytes = size;

    while (remainingBytes) {
        if((handle->status & PIPE_STATUS_ENABLED) == 0) {
            goto exit;
        }

        int64_t suspected_usedSpace = handle->head - handle->tail;
        int64_t bytesToRead = remainingBytes;
        if (suspected_usedSpace < remainingBytes) {
            bytesToRead = suspected_usedSpace;
        }

        if (bytesToRead <= 0) {
            pause();
            continue;
        }

        LOCK(&handle->lock);

        suspected_usedSpace = handle->head - handle->tail;
        bytesToRead = remainingBytes;
        if (suspected_usedSpace < remainingBytes) {
            bytesToRead = suspected_usedSpace;
        }
        
        if (bytesToRead <= 0) {
            UNLOCK(&handle->lock);
            continue;
        }

        uint64_t old_tail = handle->tail;
        handle->tail += bytesToRead;

        ring_read(handle->ring_buffer, handle->ring_buffer_size, old_tail, (char*)buffer + size - remainingBytes, bytesToRead);

        UNLOCK(&handle->lock);

        remainingBytes -= bytesToRead;
    }

exit:
    return size - remainingBytes;
}






// BELOW DOES NOT WORK

// int write(Handle _handle, const void* buffer, uint64_t size) {
//     HandleData* handle = (HandleData*)_handle;

//     uint64_t remainingBytes = size;

//     while (remainingBytes) {
//         int64_t suspected_freeSpace = handle->ring_buffer_size + __atomic_load_n(&handle->tail_commit, __ATOMIC_ACQUIRE) - __atomic_load_n(&handle->head_reserve, __ATOMIC_ACQUIRE);
//         int64_t bytesToWrite = remainingBytes;
//         if (suspected_freeSpace < remainingBytes) {
//             bytesToWrite = suspected_freeSpace;
//         }

//         if (bytesToWrite <= 0) {
//             pause();
//             if((handle->status & PIPE_STATUS_ENABLED) == 0)
//                 goto exit;
//             continue;
//         }

//         uint64_t old_head = __atomic_fetch_add(&handle->head_reserve, bytesToWrite, __ATOMIC_SEQ_CST);

//         while (old_head + bytesToWrite - __atomic_load_n(&handle->tail_commit, __ATOMIC_ACQUIRE) > handle->ring_buffer_size) {
//             // Other thread wrote and there's no space.
//             // Wait until our reserved space can be written to.
//             pause();
//             if((handle->status & PIPE_STATUS_ENABLED) == 0) {
//                 // We leave ring buffer in unstable state.
//                 // head_reserve != head_commit
//                 goto exit;
//             }
//         }

//         ring_write(handle->ring_buffer, handle->ring_buffer_size, old_head, (char*)buffer + size - remainingBytes, bytesToWrite);

//         while (__atomic_load_n(&handle->head_commit, __ATOMIC_ACQUIRE) != old_head) {
//             pause();
//             if((handle->status & PIPE_STATUS_ENABLED) == 0) {
//                 // We leave ring buffer in unstable state.
//                 // head_reserve != head_commit
//                 goto exit;
//             }
//         }

//         // handle->head_commit = old_head + bytesToWrite;
//         __atomic_store_n(&handle->head_commit, old_head + bytesToWrite, __ATOMIC_RELEASE);

//         remainingBytes -= bytesToWrite;
//     }

// exit:
//     return size - remainingBytes;
// }

// int read(Handle _handle, void* buffer, uint64_t size) {
//     HandleData* handle = (HandleData*)_handle;

//     // @TODO When looping check status flag. We may need sleeping.
//     uint64_t remainingBytes = size;

//     while (remainingBytes) {
//         int64_t suspected_usedSpace = __atomic_load_n(&handle->head_commit, __ATOMIC_ACQUIRE) - __atomic_load_n(&handle->tail_reserve, __ATOMIC_ACQUIRE);
//         int64_t bytesToRead = remainingBytes;
//         if (suspected_usedSpace < remainingBytes) {
//             bytesToRead = suspected_usedSpace;
//         }

//         if (bytesToRead <= 0) {
//             if((handle->status & PIPE_STATUS_ENABLED) == 0) {
//                 goto exit;
//             }
//             pause();
//             continue;
//         }

//         uint64_t old_tail = __atomic_fetch_add(&handle->tail_reserve, bytesToRead, __ATOMIC_SEQ_CST);

//         while (__atomic_load_n(&handle->head_commit, __ATOMIC_ACQUIRE) - old_tail < bytesToRead) {
//             // Other thread wrote and there's no space.
//             // Wait until our reserved space can be written to.
//             pause();
//             if((handle->status & PIPE_STATUS_ENABLED) == 0) {
//                 // We leave ring buffer in unstable state.
//                 // head_reserve != head_commit
//                 goto exit;
//             }
//         }

//         ring_read(handle->ring_buffer, handle->ring_buffer_size, old_tail, (char*)buffer + size - remainingBytes, bytesToRead);
    
//         while (__atomic_load_n(&handle->tail_commit, __ATOMIC_ACQUIRE) != old_tail) {
//             pause();
//             if((handle->status & PIPE_STATUS_ENABLED) == 0) {
//                 // We leave ring buffer in unstable state.
//                 // head_reserve != head_commit
//                 goto exit;
//             }
//         }

//         // handle->tail_commit = old_tail + bytesToRead;
//         __atomic_store_n(&handle->tail_commit, old_tail + bytesToRead, __ATOMIC_RELEASE);

//         remainingBytes -= bytesToRead;
//     }

// exit:
//     return size - remainingBytes;
// }

