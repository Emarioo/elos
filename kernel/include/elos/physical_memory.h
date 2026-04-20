#pragma once 


#include "elos/boot_api.h"

#include "elos/common/types.h"

#define PAGE_SIZE 4096

void PMEM_init(BootAPI* boot_api);


/*
    Allocate generic memory mapped memory
    Memory is uninitialized
*/
void* PMEM_allocate(u64 bytes, void* ptr);
#define PMEM_alloc(BYTES) PMEM_allocate(BYTES, NULL)
#define PMEM_free(PTR) PMEM_allocate(0, PTR)
#define PMEM_realloc(BYTES, PTR) PMEM_allocate(BYTES, PTR)

/*
    Does no memory mapping
    Memory is uninitialized
*/
void* PMEM_allocate_phys_pages(u64 requested_pages);

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

