#pragma once

#include "elos/common/types.h"

#include "elos/boot_api.h"

#define PAGE_BIT_PRESENT   (1LU << 0)
#define PAGE_BIT_WRITE     (1LU << 1)
#define PAGE_BIT_USER      (1LU << 2)
#define PAGE_BIT_PWT       (1LU << 3)
#define PAGE_BIT_PCD       (1LU << 4)
#define PAGE_BIT_ACCESSED  (1LU << 5)
#define PAGE_BIT_DIRTY     (1LU << 6)
#define PAGE_BIT_PAT       (1LU << 7)
#define PAGE_BIT_HUGE_PAGE (1LU << 7)
#define PAGE_BIT_GLOBAL    (1LU << 8)


typedef enum MapPageFlag {
    PAGING_FLAG_USE_RESERVED_TABLE = 0x1,
    PAGING_FLAG_READONLY           = 0x2,
    PAGING_FLAG_NOT_CACHED         = 0x4,
    PAGING_FLAG_USER_SPACE         = 0x8,
    PAGING_FLAG_GLOBAL_PAGE        = 0x10,
} MapPageFlag;

typedef struct Page {
    u64 entries[512];
} Page;



/*
    Sets up backup page tables in case we run out of them when mapping pages.
    At the moment the backup pages is static data in kernel identity mapped by UEFI
    so we always have access to them. If we need new tables then we can easily map up
    new ones.
*/
void init_paging(BootAPI* boot_api);

/*
    The lower 12 bits of virtual and physical address should be the exact same.
    (it is the page offset into the pages we map)

    Fails if page is already mapped.
*/
bool map_memory(Page* root, void* virtual_address, void* physical_address, u64 size, MapPageFlag flags);

/*
    Returns false if address wasn't mapped

    Note that if you unmap a virtual address that points to some physical page
    then that physical page will be lost. Unless you are bookkeeping it's address or
    have already "reclaimed" that physical page (which is done in phys_allocator).
*/
bool unmap_memory(Page* root, void* virtual_address, u64 size, MapPageFlag flags);


void* retrieve_physical_address(Page* root, void* virtual_address);

