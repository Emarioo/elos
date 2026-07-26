
// @NOTTESTED this paging code hasn't been tested much. Might not work so well.
//

#include "elos/physical_memory.h"
#include "elos/kernel/pmem/paging.h"

#include "elos/common/intrinsics.h"
#include "elos/common/types.h"
#include "elos/common/string.h"

#include "elos/kernel_console.h"

#include "elos/kernel/config.h"

#define printf(...) KCON_printf(__VA_ARGS__)


#define MASK_48_BIT 0x0000FFFFFFFFFFFFLLU
#define MASK_48_4KB_ADDRESS 0x0000FFFFFFFFF000LLU
#define MASK_48_2MB_ADDRESS 0x0000FFFFFFE00000LLU
#define MASK_48_1GB_ADDRESS 0x0000FFFFC0000000LLU



_align(4096) Page fixedTables[15];
int  fixedTables_len;

u64 dynamicTable_size = 0x200000;
u64 dynamicTable_len;
u64 dynamicTable_base;

Page* get_fixed_table() {
    if (fixedTables_len >= ARRAY_LENGTH(fixedTables)) {
        kernel_bug();
        return NULL;
    }
    Page* table = &fixedTables[fixedTables_len];
    fixedTables_len++;
    return table;
}

Page* get_dynamic_table() {
    u64 dynamicTable_cap = dynamicTable_size / sizeof(Page);
    if (dynamicTable_len >= dynamicTable_cap) {
        kernel_bug();
        return NULL;
    }
    Page* page = (Page*)(dynamicTable_base + dynamicTable_len * sizeof(Page));
    dynamicTable_len++;
    if ((uintptr_t)page & (PAGE_SIZE-1)) {
        kernel_bug();
        return NULL;
    }
    return page;
}


void init_paging(BootAPI* boot_api) {
    // Physical memory regions must have been made so that PMEM_alloc_phys is functional.

    // Map frame buffer

    // We assume EFI identically mapped physical and virtual address of the kernel.

    // @TODO Implement PCID (process context ID) and global pages for kernel.
    //   This optimizes TLB flushing when writing cr3. Or rather we don't flush TLB
    //   but when we do a virt to phys translation and the PCID of the cached address differs
    //   from current PCID in cr3 then we flush it as needed.
    //   Marking kernel pages global means they don't get flushed when PCID differs since we
    //   want them to be the same for every page table in every process. (no need to flush)

    g_kernelPageTable = get_fixed_table();
    Page* rootTable = g_kernelPageTable;

    dynamicTable_base = (u64)PMEM_alloc_phys(dynamicTable_size, PMEM_FLAG_NONE);

    map_memory(rootTable, (void*)dynamicTable_base, (void*)dynamicTable_base, dynamicTable_size, PAGING_FLAG_USE_RESERVED_TABLE);
    map_memory(rootTable, __kernel_start, __kernel_start, (u64)__kernel_end - (u64)__kernel_start, PAGING_FLAG_USE_RESERVED_TABLE | PAGING_FLAG_EXECUTABLE);
    map_memory(rootTable, __stack_start, __stack_start, (u64)__stack_end - (u64)__stack_start, PAGING_FLAG_USE_RESERVED_TABLE);

    write_cr3((u64)rootTable); // Will fully flush TLB

    memset((void*)dynamicTable_base, 0, dynamicTable_size);

    // These might be large so we don't use fixed/reserved page tables.
    if (boot_api->frame_buffer_base) {
        map_memory(rootTable, boot_api->frame_buffer_base, boot_api->frame_buffer_base, boot_api->frame_buffer_size, PAGING_FLAG_NOT_CACHED);
    }
    if (boot_api->initrd) {
        map_memory(rootTable, boot_api->initrd, boot_api->initrd, boot_api->initrd_size, 0);
    }
}

bool map_memory(Page* root, void* virtual_address, void* physical_address, u64 size, MapPageFlag flags) {
    u64 user_bit = 0;
    u64 write_bit = PAGE_BIT_WRITE;
    u64 cache_bit = 0;
    u64 exec_bit = PAGE_BIT_XD;
    if (flags & PAGING_FLAG_READONLY) {
        write_bit = 0;
    }
    if (flags & PAGING_FLAG_NOT_CACHED) {
        cache_bit = PAGE_BIT_PCD;
    }
    if (flags & PAGING_FLAG_USER_SPACE) {
        user_bit = PAGE_BIT_USER;
    }
    if (flags & PAGING_FLAG_EXECUTABLE) {
        exec_bit = 0;
    }

    u64 voff = (u64)virtual_address & 0xFFF;
    u64 poff = (u64)physical_address & 0xFFF;
    u64 flooredSize = size;
    if (voff != 0 || poff != 0) {
        if (voff > poff) {
            flooredSize += voff;
        } else {
            flooredSize += poff;
        }
    }

    u64 bytes_left = ((flooredSize + PAGE_SIZE-1) / PAGE_SIZE) * PAGE_SIZE;
    u64 virt = (u64)virtual_address & MASK_48_4KB_ADDRESS;
    u64 phys = (u64)physical_address & MASK_48_4KB_ADDRESS;

    #define get_table() (flags & PAGING_FLAG_USE_RESERVED_TABLE ? get_fixed_table() : get_dynamic_table())

    while (bytes_left > 0) {
        int lvl4 = (virt >> 39) & 0x1FF;
        int lvl3 = (virt >> 30) & 0x1FF;
        int lvl2 = (virt >> 21) & 0x1FF;
        int lvl1 = (virt >> 12) & 0x1FF;

        u64 entry4 = root->entries[lvl4];
        if ((entry4 & PAGE_BIT_PRESENT) == 0) {
            // Get physical page and map it in.
            Page* page = get_table();
            if (!page) {
                return false;
            }
            entry4 = PAGE_BIT_PRESENT | write_bit | user_bit | ((u64)page & MASK_48_4KB_ADDRESS);
            root->entries[lvl4] = entry4;
        } else {
            root->entries[lvl4] = entry4 | write_bit | user_bit;
        }

        Page* page_table_3 = (Page*)(entry4 & MASK_48_4KB_ADDRESS);
        u64 entry3 = page_table_3->entries[lvl3];

        #define GB (0x40000000)
        #define MB (0x100000)
        
        if (bytes_left >= GB && (virt & (GB-1)) == 0 && (phys & (GB-1)) == 0) {
            // We can make 1 GB page if
            //   - We have at least 1 GB left to map
            //   - The next virtual and physical addresses are aligned by 1 GB
            
            // We are leaking page table here when we ovewrite the entry.

            entry3 = PAGE_BIT_PRESENT | cache_bit | exec_bit | write_bit | user_bit | PAGE_BIT_HUGE_PAGE | (MASK_48_1GB_ADDRESS & phys);
            page_table_3->entries[lvl3] = entry3;
            
            flush_tlb_entry((void*)virt);
            virt += GB;
            phys += GB;
            bytes_left -= GB;
            continue;
        }

        if ((entry3 & PAGE_BIT_PRESENT) == 0 || (entry3 & PAGE_BIT_HUGE_PAGE)) {
            // Get physical page and map it in.
            Page* page = get_table();
            if (!page) {
                return false;
            }
            entry3 = PAGE_BIT_PRESENT | PAGE_BIT_WRITE | user_bit | ((u64)page & MASK_48_4KB_ADDRESS);
            page_table_3->entries[lvl3] = entry3;
        } else {
            page_table_3->entries[lvl3] = entry3 | write_bit | user_bit;
        }
        
        Page* page_table_2 = (Page*)(entry3 & MASK_48_4KB_ADDRESS);
        u64 entry2 = page_table_2->entries[lvl2];

        // @NOCHECKIN Doom runs when disabling huge pages.
        // if (bytes_left >= 2*MB && (virt & (2*MB-1)) == 0 && (phys & (2*MB-1)) == 0) {
        //     // We can make 2 MB page if
        //     //   - We have at least 2 MB left to map
        //     //   - The next virtual and physical addresses are aligned by 2 MB
            
        //     // We are leaking page table here when we ovewrite the entry.

        //     entry2 = PAGE_BIT_PRESENT | cache_bit | exec_bit | write_bit | user_bit | PAGE_BIT_HUGE_PAGE | (MASK_48_2MB_ADDRESS & phys);
        //     page_table_2->entries[lvl2] = entry2;
            
        //     flush_tlb_entry((void*)virt);
        //     virt += 2*MB;
        //     phys += 2*MB;
        //     bytes_left -= 2*MB;
        //     continue;
        // }

        if ((entry2 & PAGE_BIT_PRESENT) == 0 || (entry2 & PAGE_BIT_HUGE_PAGE)) {
            // Get physical page and map it in.
            Page* page = get_table();
            if (!page) {
                return false;
            }
            entry2 = PAGE_BIT_PRESENT | PAGE_BIT_WRITE | user_bit | ((u64)page & MASK_48_4KB_ADDRESS);
            page_table_2->entries[lvl2] = entry2;
        } else {
            page_table_2->entries[lvl2] = entry2 | write_bit | user_bit;
        }

        Page* page_table_1 = (Page*)(entry2 & MASK_48_4KB_ADDRESS);
        u64 entry1 = page_table_1->entries[lvl1];

        entry1 = PAGE_BIT_PRESENT | cache_bit | exec_bit | write_bit | user_bit | (phys & MASK_48_4KB_ADDRESS);

        page_table_1->entries[lvl1] = entry1;

        flush_tlb_entry((void*)virt);
        virt += PAGE_SIZE;
        phys += PAGE_SIZE;
        bytes_left -= PAGE_SIZE;
    }

    return true;
}

bool unmap_memory(Page* root, void* virtual_address, u64 size, MapPageFlag flags) {
    
    return true;
}

void* retrieve_physical_address(Page* root, void* virtual_address) {
    u64 virt = (u64)virtual_address;
    int lvl4 = (virt >> 39) & 0x1FF;
    int lvl3 = (virt >> 30) & 0x1FF;
    int lvl2 = (virt >> 21) & 0x1FF;
    int lvl1 = (virt >> 12) & 0x1FF;

    u64 entry4 = root->entries[lvl4];
    if ((entry4 & PAGE_BIT_PRESENT) == 0) {
        return NULL;
    }

    Page* page_table_3 = (Page*)(entry4 & MASK_48_4KB_ADDRESS);
    u64 entry3 = page_table_3->entries[lvl3];
    if ((entry3 & PAGE_BIT_PRESENT) == 0) {
        return NULL;
    }
    if ((entry3 & PAGE_BIT_HUGE_PAGE) != 0) {
        return (void*)(entry3 & MASK_48_1GB_ADDRESS);
    }
    
    Page* page_table_2 = (Page*)(entry3 & MASK_48_4KB_ADDRESS);
    u64 entry2 = page_table_2->entries[lvl2];
    if ((entry2 & PAGE_BIT_PRESENT) == 0) {
        return NULL;
    }
    if ((entry2 & PAGE_BIT_HUGE_PAGE) != 0) {
        return (void*)(entry2 & MASK_48_2MB_ADDRESS);
    }
    
    Page* page_table_1 = (Page*)(entry2 & MASK_48_4KB_ADDRESS);
    u64 entry1 = page_table_1->entries[lvl1];
    if ((entry1 & PAGE_BIT_PRESENT) == 0) {
        return NULL;
    }
    return (void*)((entry1 & MASK_48_4KB_ADDRESS) | (virt & 0xFFF));
}
