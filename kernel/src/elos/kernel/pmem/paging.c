
// @NOTTESTED this paging code hasn't been tested much. Might not work so well.
//

#include "elos/kernel/memory/paging.h"
#include "elos/kernel/common/types.h"
#include "elos/kernel/memory/phys_allocator.h"


static inline u64 read_cr3() {
    u64 reg;
    asm (
        "mov %%cr3, %0\n"
        : "=r" (reg)
    );
    return reg;
}
static inline void write_cr3(u64 reg) {
    asm (
        "mov %0, %%cr3\n"
        :
        : "r" (reg)
    );
}
static inline void flush_tlb(void* addr) {
    asm (
        "invlpg (%0)\n"
        :
        : "r" (addr)
    );
}

// These are already identity mapped by UEFI
static u64 next_reserved_page_table;
static u64 reserved_page_table_3[512];
static u64 reserved_page_table_2[512];
static u64 reserved_page_table_1[512];
static u64* free_mappedPageTables[3];
static int free_mappedPageTables_len;


u64* fetch_page_table(u64* page_table, int index, bool alloc_missing_tables);
void* alloc_page_table();

void init_paging() {
    const u64 MASK_48_BIT = 0x0000FFFFFFFFFFFF;
    const u64 MASK_ENTRY_PHYS_ADDRESS = 0x0000FFFFFFFFF000;

    free_mappedPageTables[0] = reserved_page_table_1;
    free_mappedPageTables[1] = reserved_page_table_2;
    free_mappedPageTables[2] = reserved_page_table_3;
    free_mappedPageTables_len = 3;

    u64 base = 0x23000000;
    int lvl4 = (base >> 39) & 0x1FF;
    int lvl3 = (base >> 30) & 0x1FF;
    int lvl2 = (base >> 21) & 0x1FF;
    int lvl1 = (base >> 12) & 0x1FF;

    u64* page_table_4 = (void*)read_cr3();

    u64 entry;

    entry = page_table_4[lvl4];
    if ((entry & 1) == 0) {
        // clear previous flags
        entry = 3; // set present, read/write bit, clear user to get supervisor page (for now)
        // Allocate Page Table
        // Put new table into entry
        entry |= (u64)free_mappedPageTables[free_mappedPageTables_len--] & MASK_ENTRY_PHYS_ADDRESS; // bits 12 - 48
        // Put table entry back into page directory
        page_table_4[lvl4] = entry;
    }
    u64* page_table_3 = (u64*)(entry & MASK_ENTRY_PHYS_ADDRESS);

    entry = page_table_3[lvl3];
    if ((entry & 1) == 0) {
        // clear previous flags
        entry = 3; // set present, read/write bit, clear user to get supervisor page (for now)
        // Allocate Page Table
        // Put new table into entry
        entry |= (u64)free_mappedPageTables[free_mappedPageTables_len--] & MASK_ENTRY_PHYS_ADDRESS; // bits 12 - 48
        // Put table entry back into page directory
        page_table_3[lvl3] = entry;
    }
    u64* page_table_2 = (u64*)(entry & MASK_ENTRY_PHYS_ADDRESS);

    entry = page_table_2[lvl2];
    if ((entry & 1) == 0) {
        // clear previous flags
        entry = 3; // set present, read/write bit, clear user to get supervisor page (for now)
        // Allocate Page Table
        // Put new table into entry
        entry |= (u64)free_mappedPageTables[free_mappedPageTables_len--] & MASK_ENTRY_PHYS_ADDRESS; // bits 12 - 48
        // Put table entry back into page directory
        page_table_3[lvl2] = entry;
    }
    u64* page_table_1 = (u64*)(entry & MASK_ENTRY_PHYS_ADDRESS);

    // page table can be free or whatever. It's fine.
    
    flush_tlb((void*)base);
}

void* alloc_page_table() {
    u64 base = 0x23000000;
    const u64 MASK_48_BIT = 0x0000FFFFFFFFFFFF;
    const u64 MASK_ENTRY_PHYS_ADDRESS = 0x0000FFFFFFFFF000;
    const int PAGE_SIZE = 0x1000;

    // physical address of new page
    // This is not the address we return.
    // We return one from reserved_page_table
    void* new_page_table = kerneL_alloc_phys_pages(1);
    
    u64* page_table_4 = (void*)read_cr3();

    u64 index = next_reserved_page_table;
    u64 virt;
    while (1) {
        virt = (u64)base + index * PAGE_SIZE;
        
        int lvl4 = (virt >> 39) & 0x1FF;
        int lvl3 = (virt >> 30) & 0x1FF;
        int lvl2 = (virt >> 21) & 0x1FF;
        int lvl1 = (virt >> 12) & 0x1FF;

        u64* page_table_3 = fetch_page_table(page_table_4, lvl4, false);
        if (page_table_3) {
            kernel_bug();
            return NULL;
        }
        u64* page_table_2 = fetch_page_table(page_table_3, lvl3, false);
        if (page_table_2) {
            kernel_bug();
            return NULL;
        }
        u64* page_table_1 = fetch_page_table(page_table_2, lvl2, false);
        if (page_table_1) {
            kernel_bug();
            return NULL;
        }

        u64 entry = page_table_1[lvl1];
        if ((entry & 1) == 0) {
            index++;
            continue;
        }

        if (free_mappedPageTables_len < 3) {
            // We need to refill free mapped page tables so that we don't run out of table directories
            // when allocating new page tables.
            
            void* extraTable = kerneL_alloc_phys_pages(1);

            entry = 3; // set present, read/write bit, clear user to get supervisor page (for now)
            entry |= (u64)extraTable & MASK_ENTRY_PHYS_ADDRESS;
            page_table_1[lvl1] = entry;
            free_mappedPageTables[free_mappedPageTables_len] = (u64*)virt;
            index++;
            continue;
        }

        // found free entry
        entry = 3; // set present, read/write bit, clear user to get supervisor page (for now)
        entry |= (u64)new_page_table & MASK_ENTRY_PHYS_ADDRESS;
        page_table_1[lvl1] = entry;
        break;
    }

    next_reserved_page_table = index + 1;
    flush_tlb((void*)virt);
    return (void*) virt;
}

// create page table if it doesn't exist
u64* fetch_page_table(u64* page_table, int index, bool alloc_missing_tables) {
    const u64 MASK_48_BIT = 0x0000FFFFFFFFFFFF;
    const u64 MASK_ENTRY_PHYS_ADDRESS = 0x0000FFFFFFFFF000;
    
    u64 page_table_entry = page_table[index];

    if ((page_table_entry & 1) == 0) {
        // Page table is not present

        if (!alloc_missing_tables)
            return NULL;

        // clear previous flags
        page_table_entry = 3; // set present, read/write bit, clear user to get supervisor page (for now)
                                // Clear page cache disable, page write through, Execute Disable
        
        // Allocate Page Table
        void* allocated_page_table = alloc_page_table();
        // Put new table into entry
        page_table_entry |= (u64)allocated_page_table & MASK_ENTRY_PHYS_ADDRESS; // bits 12 - 48
        // Put table entry back into page directory
        page_table[index] = page_table_entry;
    }

    u64* next_page_table = (u64*)(page_table_entry & MASK_ENTRY_PHYS_ADDRESS);
    return next_page_table;
}

bool map_page(void* virtual_address, void* physical_address) {
    const u64 MASK_48_BIT = 0x0000FFFFFFFFFFFF;
    const u64 MASK_ENTRY_PHYS_ADDRESS = 0x0000FFFFFFFFF000;

    u64 virt = (u64)virtual_address & MASK_48_BIT;
    u64 phys = (u64)physical_address & MASK_48_BIT;

    int lvl4 = (virt >> 39) & 0x1FF;
    int lvl3 = (virt >> 30) & 0x1FF;
    int lvl2 = (virt >> 21) & 0x1FF;
    int lvl1 = (virt >> 12) & 0x1FF;
    int page_offset = virt & 0xFFF;

    int phys_page_offset = phys & 0xFFF;

    if (page_offset != phys_page_offset) {
        kernel_bug();
        // Bug in kernel if this happens.
        // (only kernel calls this function)
        // Well, actually, this would happen if we run out of physical pages.
        return false;
    }

    // 512 entries in each map/directory/table
    u64* page_table_4 = (void*)read_cr3();
    u64* page_table_3 = fetch_page_table(page_table_4, lvl4, true);
    if (!page_table_3) {
        // Ran out of physical pages for page tables.
        return NULL;
    }
    u64* page_table_2 = fetch_page_table(page_table_3, lvl3, true);
    if (!page_table_2) {
        // Ran out of physical pages for page tables.
        return NULL;
    }
    u64* page_table_1 = fetch_page_table(page_table_2, lvl2, true);
    if (!page_table_1) {
        // Ran out of physical pages for page tables.
        return NULL;
    }

    u64 page_table_1_entry = page_table_1[lvl1];
    if (page_table_1_entry & 1) {
        u64 cur_phys = page_table_1_entry & MASK_ENTRY_PHYS_ADDRESS;
        // page already present and mapped
        return cur_phys == ((u64)physical_address & MASK_ENTRY_PHYS_ADDRESS);
    }

    page_table_1_entry = 3; // present, read/write, supervisor
    page_table_1_entry |= phys & MASK_ENTRY_PHYS_ADDRESS;

    page_table_1[lvl1] = page_table_1_entry;
    
    flush_tlb(virtual_address);
    return true;
}


bool unmap_page(void* virtual_address) {
    const u64 MASK_48_BIT = 0x0000FFFFFFFFFFFF;
    const u64 MASK_ENTRY_PHYS_ADDRESS = 0x0000FFFFFFFFF000;

    u64 virt = (u64)virtual_address & MASK_48_BIT;

    int lvl4 = (virt >> 39) & 0x1FF;
    int lvl3 = (virt >> 30) & 0x1FF;
    int lvl2 = (virt >> 21) & 0x1FF;
    int lvl1 = (virt >> 12) & 0x1FF;

    // 512 entries in each map/directory/table
    u64* page_table_4 = (void*)read_cr3();
    u64* page_table_3 = fetch_page_table(page_table_4, lvl4, false);
    if(!page_table_3)
        return false;
    u64* page_table_2 = fetch_page_table(page_table_3, lvl3, false);
    if(!page_table_2)
        return false;
    u64* page_table_1 = fetch_page_table(page_table_2, lvl2, false);
    if(!page_table_1)
        return false;

    u64 page_table_1_entry = page_table_1[lvl1];
    if ((page_table_1_entry & 1) == 0) {
        // page wasn't mapped
        return false;
    }

    page_table_1[lvl1] = 0; // clear page
    
    flush_tlb(virtual_address);
    return true;
}

