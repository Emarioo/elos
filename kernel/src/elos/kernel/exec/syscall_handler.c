

#include "elos/execution.h"

#include "elos/kernel/exec/internal_exec.h"


#include "elos/common/types.h"
#include "elos/common/intrinsics.h"
#include "elos/common/string.h"
#include "elos/kernel_console.h"

#include "elos/kernel/exec/read_elf.h"

#include "elos/physical_memory.h"
#include "elos/monitor.h"
#include "elos/service.h"
#include "elos/user_event.h"
#include "elos/async.h"

#include "elos/cpu.h"

#include "elos/syscalls.h"


#define printf(...) KCON_printf(__VA_ARGS__)

// @TODO Move into process resource handler?
typedef struct {
    void*  address;
    size_t size;
} HeapEntry;

static HeapEntry* allocations;
static int        allocations_len;
static int        allocations_max;
static volatile u32 heap_lock;

HeapEntry* get_heap_entry(void* address) {
    LOCK_INT(&heap_lock);
    
    HeapEntry* found = NULL;
    // @TODO SLOW!!!!
    for (int i=0;i<allocations_len;i++) {
        HeapEntry* entry = &allocations[i];
        if (entry->address == address) {
            found = entry;
            break;
        }
    }
    
    UNLOCK_INT(&heap_lock);
    
    return found;
}
bool update_heap_entry(void* old_address, void* new_address, size_t size) {
    bool returnValue = false;
    
    LOCK_INT(&heap_lock);

    if (old_address == NULL) {
        for (int i=0;i<allocations_len;i++) {
            HeapEntry* entry = &allocations[i];
            if (entry->address == new_address) {
                // New address exists
                goto exit;
            }
        }
        if (!allocations) {
            int newMax = 1000;
            allocations_len = 0;
            allocations = PMEM_alloc(sizeof(HeapEntry) * newMax);
            if (!allocations) {
                goto exit;
            }
            allocations_max = newMax;
        }
        if (allocations_len >= allocations_max) {
            int newMax = allocations_max * 2 + 100;
            void* newptr = PMEM_realloc(newMax * sizeof(HeapEntry), allocations);
            if (newptr) {
                goto exit;
            }
            allocations = newptr;
            allocations_max = newMax;
        }
        HeapEntry* entry = &allocations[allocations_len];
        allocations_len++;
        entry->address = new_address;
        entry->size = size;
        returnValue = true;
    } else {
        for (int i=0;i<allocations_len;i++) {
            HeapEntry* entry = &allocations[i];
            if (entry->address == old_address) {
                entry->address = new_address;
                entry->size = size;
                returnValue = true;
                // @TODO Check new address doesn't exist.
                goto exit;
            }
        }
    }

exit:
    UNLOCK_INT(&heap_lock);
    return returnValue;
}

u64 EXEC_syscall_handler(u64 arg0, u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5) {
    u64 returnValue = ELOS_GENERIC_ERROR;
    u64 syscall_id;
    asm volatile (
        "mov %%rax, %0"
        : "=a" (syscall_id)
    );
    // @TODO Validate parameters! Accessible address by user process. Valid sizes and lengths?

    PageTable* userPageTable = (void*)(read_cr3() & ~0xFFFLU);

    // @TODO For the user processes we need to swap out their pages so they can't
    //   access each others ELF image or HEAP or frame buffers.

    switch (syscall_id) {
        case _SYS_CAPABILITIES: {
            ELOS_Capabilities* cap = (ELOS_Capabilities*)arg0;

            // @TODO Get current process capabilities.
            memset(cap, 0, sizeof(ELOS_Capabilities));

            returnValue = ELOS_OK;

        } break;
        case _SYS_REQUEST_CAPABILITIES: {
            ELOS_Capabilities* cap = (ELOS_Capabilities*)arg0;

            // @TODO Get current process capabilities.
            memset(cap, 0, sizeof(ELOS_Capabilities));

            returnValue = ELOS_OK;

        } break;
        case _SYS_DEBUG_LOG: {
            const char* text = (const char*)arg0;
            u32         length = arg1;

            if (length > 0 && text) {
                // @TODO Out of bounds? check mapping of user application. Does it have access?
                if (text[length] == '\0') {
                    printf("%s", text);
                } else {
                    // @TODO Implement %.*s in printf
                    printf("(SYS_DEBUG_LOG %d, text not null terminated)\n", length);
                }
            }

            returnValue = ELOS_OK;
        } break;
        case _SYS_HEAP_ALLOCATE: {
            void** newAddress = (void**)arg0;
            u64    size = arg1;

            // @TODO Check capability and heap limit

            void* address = PMEM_alloc_phys(size, PMEM_FLAG_USER_SPACE);
            if (address) {
                write_cr3((u64)g_kernelPageTable);
                
                update_heap_entry(NULL, address, size);
                PMEM_map_memory(userPageTable, address, address, size, PMEM_FLAG_USER_SPACE);

                write_cr3((u64)userPageTable);
                memset(address, 0x9A, size);
                *newAddress = address;
                returnValue = ELOS_OK;
            } else {
                *newAddress = NULL;
                returnValue = ELOS_GENERIC_ERROR;
            }
        } break;
        case _SYS_HEAP_FREE: {
            void* oldAddress = (void*)arg0;
            
            // @TODO Check capability
            
            write_cr3((u64)g_kernelPageTable);
            update_heap_entry(oldAddress, NULL, 0);
            write_cr3((u64)userPageTable);
            
            PMEM_free(oldAddress);
            returnValue = ELOS_OK;
        } break;
        case _SYS_HEAP_REALLOCATE: {
            void** newAddress = (void**)arg0;
            u64    size = arg1;
            void*  oldAddress = (void*)arg2;
            
            // @TODO Check capability and heap limit
            
            write_cr3((u64)g_kernelPageTable);

            HeapEntry* heapEntry = get_heap_entry(oldAddress);
            if (!heapEntry) {
                
                void* address = PMEM_alloc_phys(size, PMEM_FLAG_USER_SPACE);
                if (!address) {
                    write_cr3((u64)userPageTable);
                    *newAddress = NULL;
                    returnValue = ELOS_GENERIC_ERROR;
                    break;
                }
                    
                update_heap_entry(NULL, address, size);
                PMEM_map_memory(userPageTable, address, address, size, PMEM_FLAG_USER_SPACE);
                
                write_cr3((u64)userPageTable);
                memset(address, 0x9A, size);
                *newAddress = address;
                returnValue = ELOS_OK;
                break;
            }
            u64 oldSize = heapEntry->size;
            
            void* address = PMEM_alloc_phys(size, PMEM_FLAG_IDENTITY_MAPPED|PMEM_FLAG_USER_SPACE);
            
            if (address) {
                update_heap_entry(oldAddress, address, size);
                write_cr3((u64)userPageTable);
                memcpy(address, oldAddress, oldSize);
                memset(address + oldSize, 0x9A, size - oldSize);
                PMEM_free(oldAddress);
                *newAddress = address;
                returnValue = ELOS_OK;
            } else {
                write_cr3((u64)userPageTable);
                *newAddress = NULL;
                returnValue = ELOS_GENERIC_ERROR;
            }
        } break;
        case _SYS_HEAP_MAP: {
            void*  virtAddress = (void*)arg0;
            u64    size = arg1;

            // @TODO Check capability and heap limit

            // @TODO I'm not sure HEAP_FREE can free memory from HEAP_MAP.
            //    It is not identity mapped. Heap free calls PMEM_free which
            //    kind of assumes kernel page table which isn't right.

            void* phys_address = PMEM_alloc_phys(size, PMEM_FLAG_USER_SPACE);
            if (phys_address) {
                write_cr3((u64)g_kernelPageTable);
                update_heap_entry(NULL, virtAddress, size);
                bool mapped = PMEM_map_memory(userPageTable, virtAddress, phys_address, size, PMEM_FLAG_USER_SPACE);
                write_cr3((u64)userPageTable);
                if (mapped) {
                    memset(virtAddress, 0x9A, size);
                    returnValue = ELOS_OK;
                } else {
                    returnValue = ELOS_GENERIC_ERROR;
                }
            } else {
                returnValue = ELOS_GENERIC_ERROR;
            }
        } break;
        case _SYS_DEFAULT_MONITOR: {
            ELOS_FrameBuffer*  frameBuffer = (void*)arg0;

            // @TODO Check capability

            write_cr3((u64)g_kernelPageTable);

            MonitorDevice devices[1];
            int count = ARRAY_LENGTH(devices);
            MON_scan_devices(devices, &count);

            if (count <= 0) {
                returnValue = ELOS_GENERIC_ERROR;
            } else {
                MON_FrameBuffer mon_frameBuffer;
                bool yes = MON_get_frame_buffer(devices[0], &mon_frameBuffer);
                if (!yes) {
                    returnValue = ELOS_GENERIC_ERROR;
                } else {
                    // @TODO Make it writethrough? Fully cached might be a bad idea?
                    bool mapped = PMEM_map_memory(userPageTable, mon_frameBuffer.phys_address, mon_frameBuffer.phys_address, mon_frameBuffer.size, PMEM_FLAG_USER_SPACE);
                    if (!mapped) {
                        returnValue = ELOS_GENERIC_ERROR;
                    } else {
                        frameBuffer->width = mon_frameBuffer.width;
                        frameBuffer->height = mon_frameBuffer.height;
                        frameBuffer->size = mon_frameBuffer.size;
                        frameBuffer->pixels_per_scan_line = mon_frameBuffer.pixels_per_scan_line;
                        frameBuffer->pixels = mon_frameBuffer.phys_address;
                        returnValue = ELOS_OK;
                    }
                }
            }

            write_cr3((u64)userPageTable); // @TODO Add PCID
        } break;
        case _SYS_TICKS_PER_SECOND: {
            u64* tps = (void*)arg0;

            // @TODO Check capability

            *tps = CPU_tsc_per_sec();

            returnValue = ELOS_OK;
        } break;
        case _SYS_SLEEP_NS: {
            u64* tps = (void*)arg0;

            // @TODO Check capability

            // @TODO Implement sleep.

            returnValue = ELOS_OK;
        } break;
        case _SYS_SERVICE_CREATE: {
            const char* name = (void*)arg0;
            ELOS_ServiceEndpoint* endpoint = (void*)arg1;
            u64 queueSize = arg2;

            int maxlen = 64;
            int name_len = strnlen(name, maxlen + 1);
            if (name_len > maxlen) {
                returnValue = ELOS_INVALID_PARAM;
                break;
            }

            // @TODO Check capability

            write_cr3((u64)g_kernelPageTable);

            const char* phys_name = PMEM_virt_to_phys(userPageTable, (void*)name);

            bool mapped = PMEM_map_memory(g_kernelPageTable, (void*)phys_name, (void*)phys_name, PAGE_SIZE, PMEM_FLAG_NONE);
            if (!mapped) {
                returnValue = ELOS_GENERIC_ERROR;
                break;
            }

            ServiceEndpoint* tmp_endpoint;
            bool result = SRV_service_create(phys_name, &tmp_endpoint, queueSize);
            
            write_cr3((u64)userPageTable);
            
            if (!result) {
                returnValue = ELOS_GENERIC_ERROR;
            } else {
                *endpoint = tmp_endpoint;
                returnValue = ELOS_OK;
            }

        } break;
        case _SYS_SERVICE_CONNECT: {
            const char* name = (void*)arg0;
            ELOS_ServiceEndpoint* endpoint = (void*)arg1;
            u64 queueSize = arg2;
            
            int maxlen = 64;
            int name_len = strnlen(name, maxlen + 1);
            if (name_len > maxlen) {
                returnValue = ELOS_INVALID_PARAM;
                break;
            }

            // @TODO Check capability

            write_cr3((u64)g_kernelPageTable);

            const char* phys_name = PMEM_virt_to_phys(userPageTable, (void*)name);

            bool mapped = PMEM_map_memory(g_kernelPageTable, (void*)phys_name, (void*)phys_name, PAGE_SIZE, PMEM_FLAG_NONE);
            if (!mapped) {
                returnValue = ELOS_GENERIC_ERROR;
                break;
            }

            ServiceEndpoint* tmp_endpoint;
            bool result = SRV_service_connect(phys_name, &tmp_endpoint, queueSize);
            
            write_cr3((u64)userPageTable);
            
            if (!result) {
                returnValue = ELOS_GENERIC_ERROR;
            } else {
                *endpoint = tmp_endpoint;
                returnValue = ELOS_OK;
            }
        } break;
        case _SYS_SERVICE_SEND: {
            ELOS_ServiceEndpoint endpoint = (void*)arg0;
            const u8* data = (void*)arg1;
            u64 size = arg2;

            // @TODO Check capability
            
            write_cr3((u64)g_kernelPageTable);

            const u8* phys_data = PMEM_virt_to_phys(userPageTable, (void*)data);
            
            bool mapped = PMEM_map_memory(g_kernelPageTable, (void*)phys_data, (void*)phys_data, PAGE_SIZE, PMEM_FLAG_NONE);
            if (!mapped) {
                returnValue = ELOS_GENERIC_ERROR;
                break;
            }

            bool result = SRV_service_send((ServiceEndpoint*)endpoint, phys_data, size);
            
            write_cr3((u64)userPageTable);

            if (!result) {
                returnValue = ELOS_GENERIC_ERROR;
            } else {
                returnValue = ELOS_OK;
            }
        } break;
        case _SYS_SERVICE_RECV: {
            ELOS_ServiceEndpoint endpoint = (void*)arg0;
            ELOS_ServiceEndpoint* senderEndpoint = (void*)arg1;
            u8** data = (void*)arg2;
            u64* size = (void*)arg3;
            u64  timeout_ns = arg4;

            // @TODO Check capability
            
            write_cr3((u64)g_kernelPageTable);

            ServiceEndpoint* tmp_senderEndpoint;
            u8* tmp_data;
            u64 tmp_size;

            bool result = SRV_service_recv((ServiceEndpoint*)endpoint, &tmp_senderEndpoint, &tmp_data, &tmp_size, timeout_ns);
            
            if (tmp_data) {
                PMEM_map_memory(userPageTable, tmp_data, tmp_data, tmp_size, PMEM_FLAG_USER_SPACE);
            }

            write_cr3((u64)userPageTable);

            if (senderEndpoint) {
                *senderEndpoint = (ELOS_ServiceEndpoint)tmp_senderEndpoint;
            }
            *data = tmp_data;
            *size = tmp_size;

            if (!result) {
                returnValue = ELOS_GENERIC_ERROR;
            } else {
                returnValue = ELOS_OK;
            }
        } break;
        case _SYS_SHARED_MEMORY_CREATE: {
            u64 size = arg0;
            ELOS_SharedMemory* handle = (void*)arg1;

            // @TODO Check capability
            
            write_cr3((u64)g_kernelPageTable);

            SharedMemory* tmp_handle;

            bool result = SRV_shared_memory_create(size, &tmp_handle);

            write_cr3((u64)userPageTable);

            if (!result) {
                returnValue = ELOS_GENERIC_ERROR;
            } else {
                *handle = (void*)tmp_handle;
                returnValue = ELOS_OK;
            }
        } break;
        case _SYS_SHARED_MEMORY_GRANT: {
            ELOS_SharedMemory handle = (void*)arg0;
            ELOS_ServiceEndpoint endpoint = (void*)arg1;

            // @TODO Check capability
            
            write_cr3((u64)g_kernelPageTable);

            bool result = SRV_shared_memory_grant((SharedMemory*)handle, (ServiceEndpoint*)endpoint);

            write_cr3((u64)userPageTable);

            if (!result) {
                returnValue = ELOS_GENERIC_ERROR;
            } else {
                returnValue = ELOS_OK;
            }
        } break;
        case _SYS_SHARED_MEMORY_INFO: {
            ELOS_SharedMemory handle = (void*)arg0;
            void** buffer = (void**)arg1;
            u64* size = (void*)arg2;

            // @TODO Check capability
            
            write_cr3((u64)g_kernelPageTable);

            void* tmp_buffer;
            u64 tmp_size;
            bool result = SRV_shared_memory_info((SharedMemory*)handle, &tmp_buffer, &tmp_size);

            if (result) {
                bool mapped = PMEM_map_memory(userPageTable, tmp_buffer, tmp_buffer, tmp_size, PMEM_FLAG_USER_SPACE);
                if (!mapped) {
                    returnValue = ELOS_GENERIC_ERROR;
                    write_cr3((u64)userPageTable);
                    break;
                }
            }

            write_cr3((u64)userPageTable);

            if (!result) {
                returnValue = ELOS_GENERIC_ERROR;
            } else {
                if (buffer) {
                    *buffer = tmp_buffer;
                }
                if (size) {
                    *size = tmp_size;
                }
                returnValue = ELOS_OK;
            }
        } break;
        case _SYS_REQUEST_USER_EVENT_BUFFER: {
            u32 minimumEvents = arg0;
            ELOS_UserEventBuffer** buffer = (void*)arg1;

            // @TODO Check capability
            
            write_cr3((u64)g_kernelPageTable);

            u64 wholeBufferSize;
            ELOS_UserEventBuffer* tmp_buffer;
            bool result = EVE_request_user_event_buffer(minimumEvents, &tmp_buffer, &wholeBufferSize);

            if (result) {
                bool mapped = PMEM_map_memory(userPageTable, tmp_buffer, tmp_buffer, wholeBufferSize, PMEM_FLAG_USER_SPACE);
                if (!mapped) {
                    returnValue = ELOS_GENERIC_ERROR;
                    write_cr3((u64)userPageTable);
                    break;
                }
            }

            write_cr3((u64)userPageTable);

            if (!result) {
                returnValue = ELOS_GENERIC_ERROR;
            } else {
                if (buffer) {
                    *buffer = (void*)tmp_buffer;
                }
                returnValue = ELOS_OK;
            }
        } break;
        case _SYS_CREATE_ASYNC_RING: {
            u32 maxEntries = arg0;
            ELOS_AsyncCreateFlag flags = arg1;
            ELOS_AsyncRequestRing** requestRing = (void*)arg2;
            ELOS_AsyncCompletionRing** completionRing = (void*)arg3;

            // @TODO Check capability
            
            write_cr3((u64)g_kernelPageTable);

            ELOS_AsyncRequestRing* tmp_requestRing = NULL;
            ELOS_AsyncCompletionRing* tmp_completionRing = NULL;
            int result = ASYNC_create_async_rings(maxEntries, flags, &tmp_requestRing, &tmp_completionRing);

            u32 actualMaxEntries = ringMaskFromEntryCount(maxEntries) + 1;
            u64 requestRingSize = sizeof(ELOS_AsyncRequestRing) + actualMaxEntries * sizeof(ELOS_AsyncRequest);
            u64 completionRingSize = sizeof(ELOS_AsyncCompletionRing) + actualMaxEntries * sizeof(ELOS_AsyncCompletion);

            if (result == ASYNC_OK) {
                bool mapped = PMEM_map_memory(userPageTable, tmp_requestRing, tmp_requestRing, requestRingSize, PMEM_FLAG_USER_SPACE);
                if (!mapped) {
                    // @TODO Leaking created ring!
                    returnValue = ELOS_GENERIC_ERROR;
                    write_cr3((u64)userPageTable);
                    break;
                }
                mapped = PMEM_map_memory(userPageTable, tmp_completionRing, tmp_completionRing, completionRingSize, PMEM_FLAG_USER_SPACE);
                if (!mapped) {
                    // @TODO Leaking created ring!
                    returnValue = ELOS_GENERIC_ERROR;
                    write_cr3((u64)userPageTable);
                    break;
                }
            }

            write_cr3((u64)userPageTable);

            if (result == ASYNC_OK) {
                *requestRing    = tmp_requestRing;
                *completionRing = tmp_completionRing;
                returnValue = ELOS_OK;
            } else {
                returnValue = ELOS_GENERIC_ERROR;
            }
        } break;
        case _SYS_DESTROY_ASYNC_RING: {
            ELOS_AsyncRequestRing* requestRing = (void*)arg0;
            ELOS_AsyncCompletionRing* completionRing = (void*)arg1;

            // @TODO Check capability
            
            write_cr3((u64)g_kernelPageTable);

            u32 actualMaxEntries = 0;
            int result = ASYNC_destroy_async_rings(requestRing, completionRing, &actualMaxEntries);
            if (result != ASYNC_OK) {
                returnValue = ELOS_GENERIC_ERROR;
                break;
            }

            u64 requestRingSize = sizeof(ELOS_AsyncRequestRing) + actualMaxEntries * sizeof(ELOS_AsyncRequest);
            u64 completionRingSize = sizeof(ELOS_AsyncCompletionRing) + actualMaxEntries * sizeof(ELOS_AsyncCompletion);
            
            bool mapped = PMEM_unmap_memory(userPageTable, requestRing, requestRingSize);
            mapped = PMEM_unmap_memory(userPageTable, completionRing, completionRingSize);

            write_cr3((u64)userPageTable);

            if (result == ASYNC_OK) {
                returnValue = ELOS_OK;
            } else {
                returnValue = ELOS_GENERIC_ERROR;
            }
            
        } break;
        case _SYS_SUBMIT_ASYNC_RING: {
            ELOS_AsyncRequestRing* requestRing = (void*)arg0;

            // @TODO Check capability
            
            write_cr3((u64)g_kernelPageTable);

            int result = ASYNC_submit_async_ring(requestRing);

            write_cr3((u64)userPageTable);

            if (result == ASYNC_OK) {
                returnValue = ELOS_OK;
            } else {
                returnValue = ELOS_GENERIC_ERROR;
            }
            
        } break;
        case _SYS_WAIT_ASYNC_RING: {
            ELOS_AsyncCompletionRing* completionRing = (void*)arg0;
            u64 timeout_ns = arg1;

            // @TODO Check capability
            // @TODO Validate completion ring pointer
            
            int coreIndex = CPU_get_core_index();
            EXEC_Core* core = &cores[coreIndex];
            EXEC_Thread* activeThread = &core->threads[core->active_thread];

            core->rescheduleSyscall = true;
            activeThread->waitingForIO = true;
            
            // @TODO We need to consider atomics and memory ordering especially on ARM if we support it.
            // @TODO If multiple threads called or are waiting on wait async syscall then we should only
            //    Resume as many as we have completions. We can't just an atomic counter remainingCompletions
            //    because the user is not required to consume one compleition per wait syscall.
            //    It may consume all or none.
            //    We don't have multiple threads per process yet so we need to revisit later. USER syscall API
            //    probably won't change?
            
            if (completionRing->head != completionRing->tail) {
                // We have completed stuff.
                core->rescheduleSyscall = false;
                activeThread->waitingForIO = false;
            } else {
                // We need to wait for stuff to be completed.
            }

            returnValue = ELOS_OK;
            
        } break;
        default: {
            returnValue = ELOS_INVALID_SYSCALL;
        } break;
    }

    return returnValue;
}