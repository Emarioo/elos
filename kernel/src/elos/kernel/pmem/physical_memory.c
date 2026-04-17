
#include <stdint.h>

#include "elos/physical_memory.h"

#include "elos/kernel/pmem/paging.h"

#include "elos/common/string.h"


#undef PAGE_SIZE
#define PAGE_SIZE 4096

#define MAX_REGIONS 1000

typedef enum {
    FLAG_FREE = 0x1,
    FLAG_USED = 0x2,
    FLAG_VIRTUALLY_MAPPED = 0x4,
    // TODO: This flag could add 1 extra page on each side of the allocation
    //   where it's filled with 0xDC bytes. If you wrote to it when you shouldn't
    //   have we'll know. Maybe this is more of a standard library or higher OS level
    //   feature and not kernel?
    FLAG_TRASH_BOUNDARY = 0x8,
} RegionFlag;




typedef struct PhysicalMemoryRegion {
    u64 physicalStart;
    u64 virtualStart; // unused at the moment (same as physical)
    u64 pageCount;
    u32 flags; // READ,WRITE,EXECUTABLE,MEMORY MAPPED, whether virtualStart is valid
} PhysicalMemoryRegion;

u32 g_num_free_regions; // the upper bound, regions beyond this point are all "empty" (without FLAG_FREE,FLAG_USED)
u32 g_num_used_regions;
PhysicalMemoryRegion g_free_regions[MAX_REGIONS];
PhysicalMemoryRegion g_used_regions[MAX_REGIONS];


void PMEM_init(BootAPI* boot_api) {
    // g_free_regions = find_free_descriptor(MAX_REGIONS*sizeof(PhysicalMemoryRegion));
    // if (!g_free_regions) {
    //     serial_printf("phys: Can't allocate free regions\n");
    //     return false;
    // }
    // serial_printf("phys: Allocated free regions ptr: %x, size: %d\n", g_free_regions, MAX_REGIONS*sizeof(PhysicalMemoryRegion));
    
    // g_used_regions = find_free_descriptor(MAX_REGIONS*sizeof(PhysicalMemoryRegion));
    // if (!g_used_regions) {
    //     serial_printf("phys: Can't allocate used regions\n");
    //     return false;
    // }
    // serial_printf("phys: Allocated used regions ptr: %x, size: %d\n", g_used_regions, MAX_REGIONS*sizeof(PhysicalMemoryRegion));

    // memset(g_free_regions, 0x9D, MAX_REGIONS * sizeof(PhysicalMemoryRegion));
    // memset(g_used_regions, 0x9D, MAX_REGIONS * sizeof(PhysicalMemoryRegion));

    for (int i = 0; i < boot_api->regions_len; i++) {
        MemoryRegion* reg = &boot_api->regions[i];
        
        if(g_num_free_regions >= MAX_REGIONS)
            break;

        PhysicalMemoryRegion* alloc = &g_free_regions[g_num_free_regions];
        g_num_free_regions++;

        alloc->flags = FLAG_FREE;
        alloc->pageCount = reg->page_count;
        alloc->physicalStart = reg->physical_start;
        alloc->virtualStart = 0;
    }
}



void* PMEM_allocate(u64 size, void* ptr) {
    const int requested_pages = (size + PAGE_SIZE-1) / PAGE_SIZE;
    const int requested_aligned_size = ((size + PAGE_SIZE-1) / PAGE_SIZE) * PAGE_SIZE;
    // in pages, same as PhysicalMemoryRegion.virtualStart
    const u64 old_virtual_address = (u64)ptr / PAGE_SIZE;
    void* const old_aligned_ptr   = (void*)(((u64)ptr / PAGE_SIZE) * PAGE_SIZE);

    if (!size && !old_virtual_address)
        return NULL;


    if (old_virtual_address && size) {
        // RESIZE MEMORY
        
        int found_used_index = -1;
        for (int i = 0; i < g_num_used_regions; i++) {
            PhysicalMemoryRegion* alloc = &g_used_regions[i];
            if ((alloc->flags & FLAG_USED) == 0)
                continue;
            if (alloc->virtualStart != old_virtual_address)
                continue;

            found_used_index = i;
            break;
        }

        if (found_used_index == -1) {
            return NULL;
        }

        PhysicalMemoryRegion* used_alloc = &g_used_regions[found_used_index];
        const u64 old_aligned_size = used_alloc->pageCount * PAGE_SIZE;

        void* new_ptr = PMEM_allocate(requested_aligned_size, NULL);
        memcpy(new_ptr, old_aligned_ptr, old_aligned_size);
        // TODO: We are doing a loop over used regions once above and then
        //   once below when freeing old allocation. OPTIMIZE.
        //   We'll want to use a different memory allocation implementation
        //   so no point in improving this now.
        PMEM_allocate(0, old_aligned_ptr);

        return new_ptr;

    } else if (old_virtual_address) {
        // FREE MEMORY
        
        // TODO: If the ptr you passed is valid then this function should always succeed.
        //   If g_free_regions is full then we will fail however. Memory is too scattered.
        //   Also, we can't indicate whether we succeded or not. return (void*)1 to indicate
        //   free succeded seems like a bad idea. But as I said, as long as ptr is valid free
        //   always succeeds. We'll revise and improve this implementation in the future.
        
        int found_used_index = -1;
        for (int i = 0; i < g_num_used_regions; i++) {
            PhysicalMemoryRegion* alloc = &g_used_regions[i];
            if ((alloc->flags & FLAG_USED) == 0)
                continue;
            if (alloc->virtualStart != old_virtual_address)
                continue;

            found_used_index = i;
            break;
        }

        if (found_used_index == -1) {
            return NULL;
        }

        int found_free_index = -1;
        for (int i = 0; i < g_num_free_regions + 1; i++) {
            PhysicalMemoryRegion* alloc = &g_free_regions[i];
            if ((alloc->flags & FLAG_FREE) != 0)
                continue;

            found_free_index = i;
            break;
        }

        if (found_free_index == -1) {
            return NULL;
        }

        PhysicalMemoryRegion* free_alloc = &g_free_regions[found_free_index];
        PhysicalMemoryRegion* used_alloc = &g_used_regions[found_used_index];

        free_alloc->physicalStart = used_alloc->physicalStart;
        free_alloc->virtualStart  = used_alloc->virtualStart;
        free_alloc->pageCount     = used_alloc->pageCount;
        free_alloc->flags         = FLAG_FREE;

        used_alloc->flags = 0; // clear region

        if (found_free_index >= g_num_free_regions) {
            g_num_free_regions = found_free_index + 1;
        }

        for (int i = 0; i < requested_pages; i++) {
            void* addr = (char*)old_virtual_address + used_alloc->pageCount * PAGE_SIZE;
            bool yes = unmap_page(addr);
            if (!yes) {
                // Bug in kernel if this doesn't work
                return NULL;
            }
        }

        return NULL; // we return NULL on success when freeing

    } else /* if (size) */ {
        // ALLOCATE MEMORY

        int found_free_index = -1;
        for (int i = 0; i < g_num_free_regions; i++) {
            PhysicalMemoryRegion* alloc = &g_free_regions[i];
            if ((alloc->flags & FLAG_FREE) == 0)
                continue;

            if (requested_pages >= alloc->pageCount)
                continue;
            
            found_free_index = i;
            break;
        }

        if (found_free_index == -1) {
            return NULL;
        }

        int found_used_index = -1;
        for (int i = 0; i < g_num_used_regions + 1; i++) {
            PhysicalMemoryRegion* alloc = &g_used_regions[i];
            if ((alloc->flags & FLAG_USED) != 0)
                continue;

            found_used_index = i;
            break;
        }

        if (found_used_index == -1) {
            return NULL;
        }

        PhysicalMemoryRegion* free_alloc = &g_free_regions[found_free_index];
        PhysicalMemoryRegion* used_alloc = &g_used_regions[found_used_index];

        used_alloc->physicalStart = free_alloc->physicalStart;
        used_alloc->virtualStart  = free_alloc->physicalStart;
        used_alloc->pageCount     = requested_pages;
        used_alloc->flags         = FLAG_USED | FLAG_VIRTUALLY_MAPPED;

        free_alloc->pageCount     -= requested_pages;
        free_alloc->physicalStart += requested_pages;
        if (free_alloc->pageCount == 0) {
            free_alloc->flags = 0; // clear region
        }

        if (found_used_index >= g_num_used_regions) {
            g_num_used_regions = found_used_index + 1;
        }

        void* const new_ptr    = (void*)(used_alloc->virtualStart * PAGE_SIZE);
        const u64 aligned_size = ((size + PAGE_SIZE-1) / PAGE_SIZE) * PAGE_SIZE;

        // Safe to assume regions/pages from UEFI is mapped mostly?
        // Otherwise we need this:
        // for (int i = 0; i < requested_pages; i++) {
        //     void* addr = (char*)new_ptr + i * PAGE_SIZE;
        //     bool yes = map_page(addr, addr);
        //     if (!yes) {
        //         // Bug in kernel if this doesn't work
        //         return NULL;
        //     }
        // }

        // Always initialize memory. Malicious program should not be able to read freed memory from other programs.
        memset(new_ptr, 0x9D, aligned_size);

        return new_ptr;
    }
}



// static int filter_empty_regions(PhysicalMemoryRegion* regions, int count) {
//     int begin = 0;
//     int end = count-1;
//     while (begin <= end) {
//         PhysicalMemoryRegion* b = &regions[begin];
//         PhysicalMemoryRegion* e = &regions[end];

//         if (b->flags != 0) {
//             begin++;
//             continue;
//         }
//         if (e->flags == 0) {
//             end--;
//             continue;
//         }

//         *b = *e;
//         begin++;
//         end--;
//     }
//     return begin + (regions[begin].flags != 0 ? 1 : 0);
// }



// static void sort_regions(PhysicalMemoryRegion* regions, int left, int right) {
//     // quick sort (ChatGPT)
//     if (left >= right)
//         return;
    
//     u64 pivot = regions[(left + right) / 2].physicalStart;
//     int i = left;
//     int j = right;

//     while (i <= j) {
//         while (regions[i].physicalStart < pivot)
//             i++;
//         while (regions[j].physicalStart > pivot)
//             j--;
        
//         if (i <= j) {
//             // maybe some VPXOR x64 AVX2 instruction to speed up swap?
//             // It assumes PhysicalMemoryRegion is 32 bytes. And not all computers support AVX2.
//             PhysicalMemoryRegion tmp = regions[i];
//             regions[i] = regions[j];
//             regions[j] = tmp;
//             i++;
//             j--;
//         }
//     }
//     if (left < j)
//         sort_regions(regions, left, j);
//     if (i < right)
//         sort_regions(regions, i, right);
// }



// static void merge_neighbour_regions(PhysicalMemoryRegion* regions, int count) {
//     int index = 0;
//     int read_index = 1;
//     while (index < count && read_index < count) {
//         PhysicalMemoryRegion* b = &regions[index];
//         PhysicalMemoryRegion* e = &regions[read_index];
        
//         if (b->physicalStart + b->pageCount != e->physicalStart) {
//             index = read_index;
//             read_index++;
//             continue;
//         }

//         b->pageCount += e->pageCount;
//         e->flags = 0;
//         read_index++;
//     }
// }



// void kernel_compress_regions() {
//     // THIS FUNCTION HAS NOT BEEN TESTED
//     g_num_free_regions = filter_empty_regions(g_free_regions, g_num_free_regions);

//     sort_regions(g_free_regions, 0, g_num_free_regions-1);

//     merge_neighbour_regions(g_free_regions, g_num_free_regions);
// }




void* PMEM_allocate_phys_pages(u64 requested_pages) {
    if (requested_pages <= 0)
        return NULL;

    int found_free_index = -1;
    for (int i = 0; i < g_num_free_regions; i++) {
        PhysicalMemoryRegion* alloc = &g_free_regions[i];
        if ((alloc->flags & FLAG_FREE) == 0)
            continue;

        if (requested_pages < alloc->pageCount)
            continue;
        
        found_free_index = i;
        break;
    }

    if (found_free_index == -1) {
        return NULL;
    }

    int found_used_index = -1;
    for (int i = 0; i < g_num_used_regions + 1; i++) {
        PhysicalMemoryRegion* alloc = &g_used_regions[i];
        if ((alloc->flags & FLAG_USED) != 0)
            continue;

        found_used_index = i;
        break;
    }

    if (found_used_index == -1) {
        return NULL;
    }

    PhysicalMemoryRegion* free_alloc = &g_free_regions[found_free_index];
    PhysicalMemoryRegion* used_alloc = &g_used_regions[found_used_index];

    // TODO: Describe that this region isn't memory mapped
    used_alloc->physicalStart = free_alloc->physicalStart;
    used_alloc->virtualStart  = 0;
    used_alloc->pageCount     = requested_pages;
    used_alloc->flags         = FLAG_USED;

    free_alloc->pageCount     -= requested_pages;
    free_alloc->physicalStart += requested_pages;
    if (free_alloc->pageCount == 0) {
        free_alloc->flags = 0; // clear region
    }

    if (found_used_index >= g_num_used_regions) {
        g_num_used_regions = found_used_index + 1;
    }

    void* new_ptr = (void*)(used_alloc->physicalStart * PAGE_SIZE);
    return new_ptr;
}
