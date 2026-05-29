

#include "elos/execution.h"

#include "elos/kernel/exec/internal_exec.h"


#include "elos/common/types.h"
#include "elos/common/intrinsics.h"
#include "elos/common/string.h"
#include "elos/kernel_console.h"

#include "elos/kernel/exec/read_elf.h"

#include "elos/physical_memory.h"

#include "elos/cpu.h"

#include "elos/syscalls.h"


#define printf(...) KCON_printf(__VA_ARGS__)


u64 EXEC_syscall_handler(u64 arg0, u64 arg1, u64 arg2, u64 arg3) {
    u64 returnValue = ELOS_INVALID_SYSCALL;
    u64 syscall_id;
    asm volatile (
        "mov %%rax, %0"
        : "=a" (syscall_id)
    );
    // @TODO Validate parameters! Accessible address by user process. Valid sizes and lengths?
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

            void* address = PMEM_alloc_phys(size, PMEM_FLAG_IDENTITY_MAPPED|PMEM_FLAG_USER_SPACE);
            if (address) {
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
                bool mapped = PMEM_map_memory(virtAddress, phys_address, size, PMEM_FLAG_USER_SPACE);
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
    }

    return returnValue;
}