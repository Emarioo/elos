

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
#include "elos/audio.h"

#include "elos/cpu.h"

#include "elos/syscalls.h"


#define printf(...) KCON_printf(__VA_ARGS__)

// @TODO Move into process resource handler?
typedef struct {
    void*  virtAddress;
    void*  physAddress;
    size_t size;
} HeapEntry;

static HeapEntry* allocations;
static int        allocations_len;
static int        allocations_max;
static volatile u32 heap_lock;

HeapEntry* get_heap_entry(void* virtAddress) {
    LOCK_INT(&heap_lock);
    
    HeapEntry* found = NULL;
    // @TODO SLOW!!!!
    for (int i=0;i<allocations_len;i++) {
        HeapEntry* entry = &allocations[i];
        if (entry->virtAddress == virtAddress) {
            found = entry;
            break;
        }
    }
    
    UNLOCK_INT(&heap_lock);
    
    return found;
}
bool update_heap_entry(void* old_address, void* new_address, void* physAddress, size_t size) {
    bool returnValue = false;
    
    LOCK_INT(&heap_lock);

    if (old_address == NULL) {
        for (int i=0;i<allocations_len;i++) {
            HeapEntry* entry = &allocations[i];
            if (entry->virtAddress == new_address) {
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
        entry->virtAddress = new_address;
        KERNEL_PANIC(physAddress, "physAddress was NULL when creating new entry");
        entry->physAddress = physAddress;
        entry->size = size;
        returnValue = true;
    } else {
        for (int i=0;i<allocations_len;i++) {
            HeapEntry* entry = &allocations[i];
            if (entry->virtAddress == old_address) {
                entry->virtAddress = new_address;
                entry->size = size;
                if (physAddress) {
                    entry->physAddress = physAddress;
                }
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

static PMEM_Flags elos_protection_to_pmem_flags(ELOS_Heap_Protection protection) {
    PMEM_Flags flags = PMEM_FLAG_USER_SPACE;
    if (protection & ELOS_HEAP_PROT_EXEC) {
        flags |= PMEM_FLAG_EXECUTABLE;
    }
    if (0 == (protection & ELOS_HEAP_PROT_WRITE)) {
        flags |= PMEM_FLAG_READ_ONLY;
    }
    // We always have read access.
    return flags;
}

typedef struct {
    // EXEC_Thread* thread; // should be process id
    PageTable*      userPageTable;
    MON_FrameBuffer monitor;
} FrameBufferOwner;

static volatile u32 frameBufferOwners_lock;
static FrameBufferOwner frameBufferOwners[10];
static int frameBufferOwners_len;
static int frameBufferOwners_max = ARRAY_LENGTH(frameBufferOwners);
static bool use_blankMonitor;
static MON_FrameBuffer blankFrameBuffer;

void disableDefaultMonitorForUsers() {
    // kernel memory must be mapped?
    LOCK_INT(&frameBufferOwners_lock);
    if (use_blankMonitor) {
        goto exit;
    }
    use_blankMonitor = true;

    if (!blankFrameBuffer.phys_address) {
        MonitorDevice devices[1];
        int count = ARRAY_LENGTH(devices);
        MON_scan_devices(devices, &count);

        if (count <= 0) {
            printf("KABOOM mon scan device disable default monitor\n");
            while (1) asm volatile ("hlt");
        } 
        
        bool yes = MON_get_frame_buffer(devices[0], &blankFrameBuffer);
        if (!yes) {
            printf("KABOOM mon get frame buffer disable default monitor\n");
            while (1) asm volatile ("hlt");
        }
        blankFrameBuffer.phys_address = PMEM_alloc_phys(blankFrameBuffer.size, 0);
        if (!blankFrameBuffer.phys_address) {
            printf("KABOOM alloc phys for blank framebuffer\n");
            while (1) asm volatile ("hlt");
        }
    }

    for (int i=0;i<frameBufferOwners_len;i++) {
        FrameBufferOwner* owner = &frameBufferOwners[i];
        if (owner->monitor.size != blankFrameBuffer.size) {
            printf("KABOOM owner->monitor.size != blankFrameBuffer.size %d %d\n", owner->monitor.size, blankFrameBuffer.size);
            while (1) asm volatile ("hlt");
        }
        bool mapped = PMEM_map_memory(owner->userPageTable,
            owner->monitor.phys_address, blankFrameBuffer.phys_address,
            owner->monitor.size, PMEM_FLAG_USER_SPACE);
        if (!mapped) {
            printf("KABOOM disableDefaultMonitorForUsers could not map\n");
            while (1) asm volatile ("hlt");
        }
        // @TODO Flush TLB for user pages? especially on other cores?
    }
exit:
    UNLOCK_INT(&frameBufferOwners_lock);
}
void enableDefaultMonitorForUsers() {
    // kernel memory must be mapped?
    LOCK_INT(&frameBufferOwners_lock);
    if (!use_blankMonitor) {
        goto exit;
    }
    
    for (int i=0;i<frameBufferOwners_len;i++) {
        FrameBufferOwner* owner = &frameBufferOwners[i];
        if (owner->monitor.size != blankFrameBuffer.size) {
            printf("KABOOM owner->monitor.size != blankFrameBuffer.size\n");
            while (1) asm volatile ("hlt");
        }
        bool mapped = PMEM_map_memory(owner->userPageTable,
            owner->monitor.phys_address, owner->monitor.phys_address,
            owner->monitor.size, PMEM_FLAG_USER_SPACE);
        if (!mapped) {
            printf("KABOOM disableDefaultMonitorForUsers could not map\n");
            while (1) asm volatile ("hlt");
        }
        // @TODO Flush TLB for user pages? Especially on other cores?
    }

    use_blankMonitor = false;
exit:
    UNLOCK_INT(&frameBufferOwners_lock);
}



ELOS_Error getDefaultMonitor(PageTable* userPageTable, MON_FrameBuffer* monitor) {
    ELOS_Error returnValue = ELOS_ERR_UNKNOWN;
    LOCK_INT(&frameBufferOwners_lock);

    if (frameBufferOwners_len + 1 >= frameBufferOwners_max) {
        returnValue = ELOS_ERR_UNKNOWN;
        goto exit_default_monitor;
    }

    MonitorDevice devices[1];
    int count = ARRAY_LENGTH(devices);
    MON_scan_devices(devices, &count);

    if (count <= 0) {
        returnValue = ELOS_ERR_UNKNOWN;
        goto exit_default_monitor;
    } 

    bool yes = MON_get_frame_buffer(devices[0], monitor);
    if (!yes) {
        returnValue = ELOS_ERR_UNKNOWN;
        goto exit_default_monitor;
    }

    void* physAddress = monitor->phys_address;
    if (use_blankMonitor) {
        physAddress = blankFrameBuffer.phys_address;
    }

    // @TODO Make it writethrough? Fully cached might be a bad idea?
    bool mapped = PMEM_map_memory(userPageTable, monitor->phys_address, physAddress, monitor->size, PMEM_FLAG_USER_SPACE);
    if (!mapped) {
        returnValue = ELOS_ERR_UNKNOWN;
        goto exit_default_monitor;
    } else {
        FrameBufferOwner* frameBufferOwner = &frameBufferOwners[frameBufferOwners_len];
        frameBufferOwners_len++;
        frameBufferOwner->userPageTable = userPageTable;
        frameBufferOwner->monitor = *monitor;

        returnValue = ELOS_OK;
    }

exit_default_monitor:
    UNLOCK_INT(&frameBufferOwners_lock);
    return returnValue;
}

#define SET_ADDRESS_SIZE_TYPE(VAR, VAL) \
    ( thread->compatMode ? (*(u32*)(VAR) = (u32)(size_t)(VAL)) : (*(size_t*)(VAR) = (size_t)VAL) )

u64 EXEC_syscall_handler(u64 arg0, u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5) {
    u64 returnValue = ELOS_ERR_INVALID_SYSCALL;
    u64 _syscall_id;
    asm volatile (
        "mov %%rax, %0"
        : "=a" (_syscall_id)
    );
    ELOS_SyscallID syscall_id = _syscall_id;

    // u32 coreIndex = CPU_get_core_index();
    // EXEC_Core* core = &cores[coreIndex];
    // EXEC_Thread* activeThread = &core->threads[core->active_thread];

    // printf("Syscall %u (compat %d)\n", syscall_id, activeThread->compatMode);

    // @TODO Validate parameters! Accessible address by user process. Valid sizes and lengths?

    int coreIndex = CPU_get_core_index();
    EXEC_Core* core = &cores[coreIndex];
    EXEC_Thread* thread = &core->threads[core->active_thread];

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

            // @TODO check mapping of user application. Does it have access?
            if (length > 0 && text) {
                printf("%.*s", length, text);
            }

            returnValue = ELOS_OK;
        } break;
        case _SYS_HEAP_ALLOCATE: {
            void** newAddress = (void**)arg0;
            size_t    size = arg1;

            // @TODO Check capability and heap limit

            void* address = PMEM_alloc_phys(size, 0);
            if (address) {
                write_cr3((u64)g_kernelPageTable);
                
                update_heap_entry(NULL, address, address, size);
                bool mapped = PMEM_map_memory(userPageTable, address, address, size, PMEM_FLAG_USER_SPACE);
                if (!mapped) {
                    PMEM_free(address);
                    write_cr3((u64)userPageTable);
                    SET_ADDRESS_SIZE_TYPE(newAddress, NULL);
                    returnValue = ELOS_ERR_UNKNOWN;
                    break;
                }

                write_cr3((u64)userPageTable);
                memset(address, 0x9A, size);
                SET_ADDRESS_SIZE_TYPE(newAddress, address);
                returnValue = ELOS_OK;
            } else {
                SET_ADDRESS_SIZE_TYPE(newAddress, NULL);
                returnValue = ELOS_ERR_UNKNOWN;
            }

            // printf("MALLOC 0x%zx:0x%zx (%zu KB)\n", address, address + size, size / 1024);
        } break;
        case _SYS_HEAP_FREE: {
            void* oldAddress = (void*)arg0;
            
            // @TODO Check capability
            
            write_cr3((u64)g_kernelPageTable);
            update_heap_entry(oldAddress, NULL, NULL, 0);
            write_cr3((u64)userPageTable);
            
            PMEM_free(oldAddress);
            returnValue = ELOS_OK;

            // printf("FREE %zx\n", oldAddress);
        } break;
        case _SYS_HEAP_REALLOCATE: {
            void** newAddress = (void**)arg0;
            size_t    size = arg1;
            void*  oldAddress = (void*)arg2;
            
            // @TODO Check capability and heap limit
            
            write_cr3((u64)g_kernelPageTable);

            HeapEntry* heapEntry = oldAddress == NULL ? NULL : get_heap_entry(oldAddress);
            if (!heapEntry) {
                
                void* address = PMEM_alloc_phys(size, 0);
                if (!address) {
                    write_cr3((u64)userPageTable);
                    SET_ADDRESS_SIZE_TYPE(newAddress, NULL);
                    returnValue = ELOS_ERR_UNKNOWN;
                    break;
                }
                    
                update_heap_entry(NULL, address, address, size);
                PMEM_map_memory(userPageTable, address, address, size, PMEM_FLAG_USER_SPACE);
                
                write_cr3((u64)userPageTable);
                memset(address, 0x9A, size);
                SET_ADDRESS_SIZE_TYPE(newAddress, address);
                returnValue = ELOS_OK;
                break;
            }
            u64 oldSize = heapEntry->size;
            
            void* address = PMEM_alloc_phys(size, 0);
            
            if (address) {
                update_heap_entry(oldAddress, address, address, size);
                PMEM_map_memory(userPageTable, address, address, size, PMEM_FLAG_USER_SPACE);
                write_cr3((u64)userPageTable);
                memcpy(address, oldAddress, oldSize);
                memset(address + oldSize, 0x9A, size - oldSize);
                // @TODO Unmap the old memory
                //    Free might do it already?
                PMEM_free(oldAddress);
                SET_ADDRESS_SIZE_TYPE(newAddress, address);
                returnValue = ELOS_OK;
            } else {
                write_cr3((u64)userPageTable);
                SET_ADDRESS_SIZE_TYPE(newAddress, NULL);
                returnValue = ELOS_ERR_UNKNOWN;
            }

            // printf("REALLOC %zx -> %zx:%zx (%zu KB)\n", oldAddress, address, address + size, size / 1024);
        } break;
        case _SYS_HEAP_MAP: {
            void*  virtAddress = (void*)arg0;
            size_t    size = arg1;
            ELOS_Heap_Protection protection = arg2;

            // @TODO Check capability and heap limit

            // @TODO Check if virtual memory space has stuff at the memory range we want to map.
            //   Return error if so. Right now we overwrite the page tables with our new mapping
            //   which will cause a crash.

            void* phys_address = PMEM_alloc_phys(size, 0);
            if (phys_address) {
                write_cr3((u64)g_kernelPageTable);
                update_heap_entry(NULL, virtAddress, phys_address, size);

                PMEM_Flags flags = elos_protection_to_pmem_flags(protection);

                bool mapped = PMEM_map_memory(userPageTable, virtAddress, phys_address, size, flags);
                write_cr3((u64)userPageTable);
                if (mapped) {
                    memset(virtAddress, 0x9A, size);
                    returnValue = ELOS_OK;
                } else {
                    returnValue = ELOS_ERR_UNKNOWN;
                }
            } else {
                returnValue = ELOS_ERR_UNKNOWN;
            }
        } break;
        case _SYS_HEAP_PROTECT: {
            void*  virtAddress = (void*)arg0;
            size_t    size = arg1;
            ELOS_Heap_Protection protection = arg2;

            // @TODO Check capability and heap limit

            // @TODO I'm not sure HEAP_FREE can free memory from HEAP_MAP.
            //    It is not identity mapped. Heap free calls PMEM_free which
            //    kind of assumes kernel page table which isn't right.

            write_cr3((u64)g_kernelPageTable);

            HeapEntry* heapEntry = get_heap_entry(virtAddress);
            if (!heapEntry) {
                returnValue = ELOS_ERR_UNKNOWN;
            } else {
                PMEM_Flags flags = elos_protection_to_pmem_flags(protection);

                bool mapped = PMEM_map_memory(userPageTable, virtAddress, heapEntry->physAddress, size, flags);
                if (mapped) {
                    returnValue = ELOS_OK;
                } else {
                    returnValue = ELOS_ERR_UNKNOWN;
                }
            }
            write_cr3((u64)userPageTable);
        } break;
        case _SYS_DEFAULT_MONITOR: {
            ELOS_FrameBuffer*  frameBuffer = (void*)arg0;

            // @TODO Check capability

            write_cr3((u64)g_kernelPageTable);

            MON_FrameBuffer mon_frameBuffer;
            returnValue = getDefaultMonitor(userPageTable, &mon_frameBuffer);

            write_cr3((u64)userPageTable); // @TODO Add PCID
            if (returnValue == ELOS_OK) {
                frameBuffer->width = mon_frameBuffer.width;
                frameBuffer->height = mon_frameBuffer.height;
                frameBuffer->size = mon_frameBuffer.size;
                frameBuffer->pixels_per_scan_line = mon_frameBuffer.pixels_per_scan_line;
                frameBuffer->pixels = mon_frameBuffer.phys_address;
            }
            
        } break;
        case _SYS_TICKS_PER_SECOND: {
            u64* tps = (void*)arg0;

            // @TODO Check capability

            *tps = CPU_ticks_per_second();

            returnValue = ELOS_OK;
        } break;
        case _SYS_SLEEP_NS: {
            u64 sleepTime_ns = arg0;

            // @TODO Check capability

            u64 nowTick = rdtsc();
            u64 sleepTick = nowTick + (sleepTime_ns * CPU_ticks_per_second()/100) / 10000000;

            int coreIndex = CPU_get_core_index();
            EXEC_Core* core = &cores[coreIndex];
            EXEC_Thread* activeThread = &core->threads[core->active_thread];
            activeThread->sleepUntilTick = sleepTick;

            core->rescheduleSyscall = true;

            returnValue = ELOS_OK;
        } break;
        case _SYS_SERVICE_CREATE: {
            const char* name = (void*)arg0;
            ELOS_ServiceEndpoint* endpoint = (void*)arg1;
            u32 queueSize = arg2;

            int maxlen = 64;
            int name_len = strnlen(name, maxlen + 1);
            if (name_len > maxlen) {
                returnValue = ELOS_ERR_INVALID_PARAM;
                break;
            }

            // @TODO Check capability

            write_cr3((u64)g_kernelPageTable);

            const char* phys_name = PMEM_virt_to_phys(userPageTable, (void*)name);

            bool mapped = PMEM_map_memory(g_kernelPageTable, (void*)phys_name, (void*)phys_name, PAGE_SIZE, PMEM_FLAG_NONE);
            if (!mapped) {
                returnValue = ELOS_ERR_UNKNOWN;
                break;
            }

            ServiceEndpoint* tmp_endpoint;
            bool result = SRV_service_create(phys_name, &tmp_endpoint, queueSize);
            
            write_cr3((u64)userPageTable);
            
            if (!result) {
                returnValue = ELOS_ERR_UNKNOWN;
            } else {
                SET_ADDRESS_SIZE_TYPE(endpoint, tmp_endpoint);
                returnValue = ELOS_OK;
            }

        } break;
        case _SYS_SERVICE_CONNECT: {
            const char* name = (void*)arg0;
            ELOS_ServiceEndpoint* endpoint = (void*)arg1;
            u32 queueSize = arg2;
            
            int maxlen = 64;
            int name_len = strnlen(name, maxlen + 1);
            if (name_len > maxlen) {
                returnValue = ELOS_ERR_INVALID_PARAM;
                break;
            }

            // @TODO Check capability

            write_cr3((u64)g_kernelPageTable);

            const char* phys_name = PMEM_virt_to_phys(userPageTable, (void*)name);

            bool mapped = PMEM_map_memory(g_kernelPageTable, (void*)phys_name, (void*)phys_name, PAGE_SIZE, PMEM_FLAG_NONE);
            if (!mapped) {
                returnValue = ELOS_ERR_UNKNOWN;
                break;
            }

            ServiceEndpoint* tmp_endpoint;
            bool result = SRV_service_connect(phys_name, &tmp_endpoint, queueSize);
            
            write_cr3((u64)userPageTable);
            
            if (!result) {
                returnValue = ELOS_ERR_UNKNOWN;
            } else {
                SET_ADDRESS_SIZE_TYPE(endpoint, tmp_endpoint);
                returnValue = ELOS_OK;
            }
        } break;
        case _SYS_SERVICE_SEND: {
            ELOS_ServiceEndpoint endpoint = (void*)arg0;
            const u8* data = (void*)arg1;
            u32 size = arg2;

            // @TODO Check capability
            
            write_cr3((u64)g_kernelPageTable);

            const u8* phys_data = PMEM_virt_to_phys(userPageTable, (void*)data);
            
            bool mapped = PMEM_map_memory(g_kernelPageTable, (void*)phys_data, (void*)phys_data, PAGE_SIZE, PMEM_FLAG_NONE);
            if (!mapped) {
                returnValue = ELOS_ERR_UNKNOWN;
                break;
            }

            bool result = SRV_service_send((ServiceEndpoint*)endpoint, phys_data, size);
            
            write_cr3((u64)userPageTable);

            if (!result) {
                returnValue = ELOS_ERR_UNKNOWN;
            } else {
                returnValue = ELOS_OK;
            }
        } break;
        case _SYS_SERVICE_RECV: {
            ELOS_ServiceEndpoint endpoint = (void*)arg0;
            ELOS_ServiceEndpoint* senderEndpoint = (void*)arg1;
            u8** data = (void*)arg2;
            u32* size = (void*)arg3;
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
                SET_ADDRESS_SIZE_TYPE(senderEndpoint, (ELOS_ServiceEndpoint)tmp_senderEndpoint);
            }
            SET_ADDRESS_SIZE_TYPE(data, tmp_data);
            *size = tmp_size;

            if (!result) {
                returnValue = ELOS_ERR_UNKNOWN;
            } else {
                returnValue = ELOS_OK;
            }
        } break;
        case _SYS_SHARED_MEMORY_CREATE: {
            size_t size = arg0;
            ELOS_SharedMemory* handle = (void*)arg1;

            // @TODO Check capability
            
            write_cr3((u64)g_kernelPageTable);

            SharedMemory* tmp_handle;

            bool result = SRV_shared_memory_create(size, &tmp_handle);

            write_cr3((u64)userPageTable);

            if (!result) {
                returnValue = ELOS_ERR_UNKNOWN;
            } else {
                SET_ADDRESS_SIZE_TYPE(handle, tmp_handle);
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
                returnValue = ELOS_ERR_UNKNOWN;
            } else {
                returnValue = ELOS_OK;
            }
        } break;
        case _SYS_SHARED_MEMORY_INFO: {
            ELOS_SharedMemory handle = (void*)arg0;
            void** buffer = (void**)arg1;
            size_t* size = (void*)arg2;

            // @TODO Check capability
            
            write_cr3((u64)g_kernelPageTable);

            void* tmp_buffer;
            u64 tmp_size;
            bool result = SRV_shared_memory_info((SharedMemory*)handle, &tmp_buffer, &tmp_size);

            if (result) {
                bool mapped = PMEM_map_memory(userPageTable, tmp_buffer, tmp_buffer, tmp_size, PMEM_FLAG_USER_SPACE);
                if (!mapped) {
                    returnValue = ELOS_ERR_UNKNOWN;
                    write_cr3((u64)userPageTable);
                    break;
                }
            }

            write_cr3((u64)userPageTable);

            if (!result) {
                returnValue = ELOS_ERR_UNKNOWN;
            } else {
                if (buffer) {
                    SET_ADDRESS_SIZE_TYPE(buffer, tmp_buffer);
                }
                if (size) {
                    SET_ADDRESS_SIZE_TYPE(size, tmp_size);
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
                    returnValue = ELOS_ERR_UNKNOWN;
                    write_cr3((u64)userPageTable);
                    break;
                }
            }

            write_cr3((u64)userPageTable);

            if (!result) {
                returnValue = ELOS_ERR_UNKNOWN;
            } else {
                if (buffer) {
                    SET_ADDRESS_SIZE_TYPE(buffer, (void*)tmp_buffer);
                }
                returnValue = ELOS_OK;
            }
        } break;
        case _SYS_CREATE_ASYNC_RINGS: {
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
                    returnValue = ELOS_ERR_UNKNOWN;
                    write_cr3((u64)userPageTable);
                    break;
                }
                mapped = PMEM_map_memory(userPageTable, tmp_completionRing, tmp_completionRing, completionRingSize, PMEM_FLAG_USER_SPACE);
                if (!mapped) {
                    // @TODO Leaking created ring!
                    returnValue = ELOS_ERR_UNKNOWN;
                    write_cr3((u64)userPageTable);
                    break;
                }
            }

            write_cr3((u64)userPageTable);

            if (result == ASYNC_OK) {
                SET_ADDRESS_SIZE_TYPE(requestRing, tmp_requestRing);
                SET_ADDRESS_SIZE_TYPE(completionRing, tmp_completionRing);
                returnValue = ELOS_OK;
            } else {
                returnValue = ELOS_ERR_UNKNOWN;
            }
        } break;
        case _SYS_DESTROY_ASYNC_RINGS: {
            ELOS_AsyncRequestRing* requestRing = (void*)arg0;
            ELOS_AsyncCompletionRing* completionRing = (void*)arg1;

            // @TODO Check capability
            
            write_cr3((u64)g_kernelPageTable);

            u32 actualMaxEntries = 0;
            int result = ASYNC_destroy_async_rings(requestRing, completionRing, &actualMaxEntries);
            if (result != ASYNC_OK) {
                returnValue = ELOS_ERR_UNKNOWN;
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
                returnValue = ELOS_ERR_UNKNOWN;
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
                returnValue = ELOS_ERR_UNKNOWN;
            }
            
        } break;
        case _SYS_WAIT_ASYNC_RING: {
            ELOS_AsyncCompletionRing* completionRing = (void*)arg0;
            u64 timeout_ns = arg1;

            // @TODO Check capability
            // @TODO Validate completion ring pointer

            // @TODO Handle timeout
            
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
        case _SYS_EXIT: {
            int exitCode = arg0;

            int coreIndex = CPU_get_core_index();
            EXEC_Core* core = &cores[coreIndex];
            EXEC_Thread* activeThread = &core->threads[core->active_thread];

            // @TODO Free resources.

            core->rescheduleSyscall = true;
            activeThread->used = false;
            
            returnValue = ELOS_OK;
            
        } break;

        case _SYS_DEFAULT_AUDIO: {
            ELOS_AudioDevice*  device = (void*)arg0;

            // @TODO Check capability

            write_cr3((u64)g_kernelPageTable);

            ELOS_AudioDevice dev = AUDIO_default_device();

            write_cr3((u64)userPageTable);
            
            if (dev) {
                SET_ADDRESS_SIZE_TYPE(device, dev);
                returnValue = ELOS_OK;
            } else {
                returnValue = ELOS_ERR_NO_AUDIO_DEVICE;
            }
            
        } break;
        
        case _SYS_AUDIO_INFO: {
            ELOS_AudioDevice      device     = (void*)arg0;
            ELOS_AudioDeviceInfo* deviceInfo = (void*)arg1;

            // @TODO Check capability
            // @TODO Validate device and info pointer

            write_cr3((u64)g_kernelPageTable);

            ELOS_AudioDeviceInfo tmp_deviceInfo;

            bool yes = AUDIO_get_info(device, &tmp_deviceInfo);

            write_cr3((u64)userPageTable);

            if (yes) {
                *deviceInfo = tmp_deviceInfo;
                returnValue = ELOS_OK;
            } else {
                returnValue = ELOS_ERR_UNKNOWN;
            }
            
        } break;

        case _SYS_CREATE_AUDIO_BUFFER: {
            ELOS_AudioDevice      device     = (void*)arg0;
            ELOS_AudioFormat*     format     = (void*)arg1;
            u32                   bufferSize = (u64)arg2;
            ELOS_AudioBuffer**    buffer     = (void*)arg3;

            // @TODO Check capability
            // @TODO Validate device, buffer size, format pointer, buffer pointer

            ELOS_AudioFormat tmp_format = *format;
            
            write_cr3((u64)g_kernelPageTable);

            ELOS_AudioBuffer* tmp_buffer;

            u32 memoryFootprint = sizeof(ELOS_AudioBuffer) + bufferSize;

            ELOS_Error error = AUDIO_create_buffer(device, &tmp_format, bufferSize, &tmp_buffer);
            if (error != ELOS_OK) {
                returnValue = error;
                break;
            }

            bool mapped = PMEM_map_memory(userPageTable, tmp_buffer, tmp_buffer, memoryFootprint, PMEM_FLAG_USER_SPACE);
            if (!mapped) {
                // @TODO Leaking audio buffer here.
                returnValue = ELOS_ERR_UNKNOWN;
            } else {
                returnValue = ELOS_OK;
            }
            write_cr3((u64)userPageTable);

            SET_ADDRESS_SIZE_TYPE(buffer, tmp_buffer);
            
        } break;

        // case _SYS_AUDIO_CONTROL: {
        //     // @TODO Implement
        //    returnValue = ELOS_INVALID_PARAM;
        // } break;

        case _SYS_DESTROY_AUDIO_BUFFER: {
            // @TODO Implement
            returnValue = ELOS_ERR_UNKNOWN;
        } break;

    }

    return returnValue;
}