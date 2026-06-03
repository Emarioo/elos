

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

#include "elos/cpu.h"

#include "elos/syscalls.h"


#define printf(...) KCON_printf(__VA_ARGS__)


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

            PMEM_free(oldAddress);
            returnValue = ELOS_OK;
        } break;
        case _SYS_HEAP_REALLOCATE: {
            // void** newAddress = (void**)arg0;
            // u64    size = arg1;
            // void*  oldAddress = (void*)arg2;

            // // @TODO Check capability and heap limit

            // u64 oldSize = ?;

            // void* address = PMEM_alloc_phys(size, PMEM_FLAG_IDENTITY_MAPPED|PMEM_FLAG_USER_SPACE);

            // if (address) {
            //     memcpy(address, oldAddress, oldSize);
            //     memset(address + oldSize, 0x9A, size - oldSize);
            //     PMEM_free(oldAddress);
            //     *newAddress = address;
            //     returnValue = ELOS_OK;
            // } else {
            //     *newAddress = NULL;
            // }
            returnValue = ELOS_GENERIC_ERROR;
        } break;
        case _SYS_HEAP_MAP: {
            void*  virtAddress = (void*)arg0;
            u64    size = arg1;

            // @TODO Check capability and heap limit

            void* phys_address = PMEM_alloc_phys(size, PMEM_FLAG_USER_SPACE);
            if (phys_address) {
                write_cr3((u64)g_kernelPageTable);
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
            ELOS_SharedMemoryHandle* handle = (void*)arg1;

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
            ELOS_SharedMemoryHandle handle = (void*)arg0;
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
            ELOS_SharedMemoryHandle handle = (void*)arg0;
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
        default: {
            returnValue = ELOS_INVALID_SYSCALL;
        } break;
    }

    return returnValue;
}