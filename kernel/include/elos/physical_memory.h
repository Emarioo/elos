#pragma once 


#include "elos/boot_api.h"

#include "elos/common/types.h"

#define PAGE_SIZE 4096

typedef enum PMEM_Flags {
    PMEM_FLAG_NONE,
    PMEM_FLAG_IDENTITY_MAPPED = 0x1,
    PMEM_FLAG_NOT_CACHED      = 0x2,
    PMEM_FLAG_READ_ONLY       = 0x4,
    PMEM_FLAG_EXECUTABLE      = 0x8,
    PMEM_FLAG_USER_SPACE      = 0x10,
    PMEM_FLAG_BOOT_RESERVE    = 0x20,
} PMEM_Flags;


typedef struct Page {
    u64 entries[512];
} Page;

typedef Page PageTable;

extern PageTable* g_kernelPageTable;

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

PageTable* PMEM_allocPageTable();

// @TODO Cacheable, prefetachable, write through flags.
/*
    The lower 12 bits of virtual and physical address should be the exact same.
    (it is the page offset into the pages we map)

    Fails if page is already mapped.
*/
bool PMEM_map_memory(PageTable* table, void* virtual_address, void* physical_address, u64 size, PMEM_Flags flags);

/*
    Returns false if address wasn't mapped

    Note that if you unmap a virtual address that points to some physical page
    then that physical page will be lost. Unless you are bookkeeping it's address or
    have already "reclaimed" that physical page (which is done in phys_allocator).
*/
bool PMEM_unmap_memory(PageTable* table, void* virtual_address, u64 size);

void* PMEM_virt_to_phys(PageTable* table, void* virtual_address);
