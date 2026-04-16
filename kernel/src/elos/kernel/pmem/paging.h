#pragma once

#include "elos/kernel/common/types.h"

/*
    Sets up backup page tables in case we run out of them when mapping pages.
    At the moment the backup pages is static data in kernel identity mapped by UEFI
    so we always have access to them. If we need new tables then we can easily map up
    new ones.
*/
void init_paging();

/*
    The lower 12 bits of virtual and physical address should be the exact same.
    (it is the page offset into the pages we map)

    Fails if page is already mapped.
*/
bool map_page(void* virtual_address, void* physical_address);
/*
    Returns false if address wasn't mapped

    Note that if you unmap a virtual address that points to some physical page
    then that physical page will be lost. Unless you are bookkeeping it's address or
    have already "reclaimed" that physical page (which is done in phys_allocator).
*/
bool unmap_page(void* virtual_address);
