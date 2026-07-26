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
#define PAGE_BIT_ENTRY_PAT (1LU << 7)
#define PAGE_BIT_DIR_PAT   (1LU << 12)
#define PAGE_BIT_HUGE_PAGE (1LU << 7)
#define PAGE_BIT_GLOBAL    (1LU << 8)
#define PAGE_BIT_XD        (1LU << 63)



/*
    Sets up backup page tables in case we run out of them when mapping pages.
    At the moment the backup pages is static data in kernel identity mapped by UEFI
    so we always have access to them. If we need new tables then we can easily map up
    new ones.
*/
void init_paging(BootAPI* boot_api);
