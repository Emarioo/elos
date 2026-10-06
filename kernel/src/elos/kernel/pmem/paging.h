#pragma once

#include "elos/common/types.h"

#include "elos/boot_api.h"

#define PAGE_BIT_PRESENT   ((size_t)1 << 0)
#define PAGE_BIT_WRITE     ((size_t)1 << 1)
#define PAGE_BIT_USER      ((size_t)1 << 2)
#define PAGE_BIT_PWT       ((size_t)1 << 3)
#define PAGE_BIT_PCD       ((size_t)1 << 4)
#define PAGE_BIT_ACCESSED  ((size_t)1 << 5)
#define PAGE_BIT_DIRTY     ((size_t)1 << 6)
#define PAGE_BIT_ENTRY_PAT ((size_t)1 << 7)
#define PAGE_BIT_DIR_PAT   ((size_t)1 << 12)
#define PAGE_BIT_HUGE_PAGE ((size_t)1 << 7)
#define PAGE_BIT_GLOBAL    ((size_t)1 << 8)
#define PAGE_BIT_XD        ((size_t)1 << 63)



/*
    Sets up backup page tables in case we run out of them when mapping pages.
    At the moment the backup pages is static data in kernel identity mapped by UEFI
    so we always have access to them. If we need new tables then we can easily map up
    new ones.
*/
void init_paging(BootAPI* boot_api);
