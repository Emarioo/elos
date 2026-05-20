#pragma once 


#include "elos/boot_api.h"

#include "elos/common/types.h"

#define PAGE_SIZE 4096

typedef enum PMEM_Flags {
    PMEM_FLAG_NONE,
    PMEM_FLAG_IDENTITY_MAPPED = 0x1,
    PMEM_FLAG_NOT_CACHED      = 0x2,
    PMEM_FLAG_READ_ONLY       = 0x4,
} PMEM_Flags;

void PMEM_init(BootAPI* boot_api);


/*
    Allocate generic mapped memory.
    Memory may not be identity mapped to physical addresses.
    Memory is uninitialized.
*/
void* PMEM_allocate(u64 bytes, void* ptr);
#define PMEM_alloc(BYTES) PMEM_allocate(BYTES, NULL)
#define PMEM_free(PTR) PMEM_allocate(0, PTR)
#define PMEM_realloc(BYTES, PTR) PMEM_allocate(BYTES, PTR)

/*
    Does no memory mapping by default (flags can change this)
    Memory is uninitialized
*/
void* PMEM_alloc_phys(u64 size, PMEM_Flags flags);

// @TODO Cacheable, prefetachable, write through flags.
bool PMEM_map_memory(void* virtual_address, void* physical_address, u64 size, PMEM_Flags flags);

bool PMEM_unmap_memory(void* virtual_address, u64 size);

void* PMEM_virt_to_phys(void* virtual_address);

/*
    TODO: Allocates contiguous pages with some optional flags
    
        Option to choose how memory is initialized. Zero initialized or some specific byte like 0xCD
        (allocated pages are always initialized, we don't want to leave leftover data which a malicious program read)
        DEBUG boundaries. Allocating 2 extra pages at start and end where page is filled with 0xCD. If they were modified
        we'll know and can notify the user. (user might call kernel_allocated_pages_wrote_to_debug_page() to check it)
        READ,WRITE,EXECUTE permissions. Well, you would maybe start with READ,WRITE,EXEC but then using kernel_set_flags
        turn off write permissions or something.


*/

// void* kerneL_alloc_pages(u64 size, void* ptr, u32 flags);





// /*
//     Flags specify write/read/execute access and other flags (zero initialized or 0x9D initialized)
//     Address and bytes must be page-aligned
// */
// bool kernel_vmap(void* requested_virtual_addr, u64 bytes, int flags);

// /*
//     Address and bytes must be page-aligned
// */
// bool kernel_vunmap(void* requested_virtual_addr, u64 bytes);

