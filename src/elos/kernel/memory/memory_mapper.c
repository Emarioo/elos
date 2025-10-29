/*
    This is a simple slow implementation

    TODO: Implementation assumes we won't reach MAX_REGIONS
      which we must fix.

    At the moment a "Region" is the same as an "Allocation"
*/

#include "elos/kernel/memory/memory_mapper.h"
#include "elos/kernel/common/string.h"
#include "elos/kernel/common/core_data.h"

#undef PAGE_SIZE
#define PAGE_SIZE 4096


#define MAX_REGIONS 100000

#define FLAG_FREE 0x1
#define FLAG_USED 0x2
// TODO: This flag could add 1 extra page on each side of the allocation
//   where it's filled with 0xDC bytes. If you wrote to it when you shouldn't
//   have we'll know. Maybe this is more of a standard library or higher OS level
//   feature and not kernel?
#define FLAG_TRASH_BOUNDARY 0x4


MemoryMapper g_memory_mapper;

typedef struct Region {
    u64 physicalStart;
    u64 virtualStart; // unused at the moment (same as physical)
    u64 pageCount;
    u32 flags; // READ,WRITE,EXECUTABLE
} Region;


u32 g_num_free_regions; // the upper bound, regions beyond this point are all "empty" (without FLAG_FREE,FLAG_USED)
u32 g_num_used_regions;
Region* g_free_regions;
Region* g_used_regions;



static void* find_free_descriptor(int size) {
    const int requested_pages = (size + PAGE_SIZE-1) / PAGE_SIZE;
    const int desc_count = g_memory_mapper.total_size_of_descriptors/g_memory_mapper.descriptor_size;
    for (int i = 0; i < desc_count; i++) {
        EFI_MEMORY_DESCRIPTOR* desc = (EFI_MEMORY_DESCRIPTOR*)((char*)
            g_memory_mapper.memory_descriptors + i*g_memory_mapper.descriptor_size);
        
        if (desc->Type != EfiConventionalMemory)
            continue;

        if (desc->NumberOfPages < requested_pages)
            continue;
        
        desc->NumberOfPages -= requested_pages;
        desc->PhysicalStart += requested_pages;
        desc->VirtualStart  += requested_pages;

        return (char*)desc->PhysicalStart;
    }
    return NULL;
}

bool kernel_init_memory_mapper() {
    g_free_regions = find_free_descriptor(MAX_REGIONS*sizeof(Region));
    if (!g_free_regions)
        return false;
    g_used_regions = find_free_descriptor(MAX_REGIONS*sizeof(Region));
    if (!g_used_regions)
        return false;

    const int desc_count = g_memory_mapper.total_size_of_descriptors/g_memory_mapper.descriptor_size;
    for (int i = 0; i < desc_count; i++) {
        EFI_MEMORY_DESCRIPTOR* desc = (EFI_MEMORY_DESCRIPTOR*)((char*)
            g_memory_mapper.memory_descriptors + i*g_memory_mapper.descriptor_size);
        
        if (desc->Type != EfiConventionalMemory)
            continue;

        // TODO: Do something with attributes?

        Region* alloc = &g_free_regions[g_num_free_regions];
        g_num_free_regions++;

        alloc->flags = FLAG_FREE;
        alloc->pageCount = desc->NumberOfPages;
        alloc->physicalStart = desc->PhysicalStart;
        alloc->virtualStart = desc->VirtualStart;
    }
    return true;
}



void* kernel_alloc(const u64 size, void* const ptr) {
    const int requested_pages = (size + PAGE_SIZE-1) / PAGE_SIZE;
    const int requested_aligned_size = ((size + PAGE_SIZE-1) / PAGE_SIZE) * PAGE_SIZE;
    // in pages, same as Region.virtualStart
    const u64 old_virtual_address = (u64)ptr / PAGE_SIZE;
    void* const old_aligned_ptr   = (void*)(((u64)ptr / PAGE_SIZE) * PAGE_SIZE);

    if (!size && !old_virtual_address)
        return NULL;

    // @TODO: Temporary, when we're in BootServices we do this. (we are when loading font when reading files)
    //   Can't read files after BootServices yet.
    if (!kernel__core_data->exited_bootservices) {
        if (size && !old_virtual_address) {
            void* new_addr;
            EFI_STATUS status = ST->BootServices->AllocatePages(AllocateAnyPages, EfiLoaderData, requested_pages, (EFI_PHYSICAL_ADDRESS*)&new_addr);
            if (EFI_ERROR(status)) {
                return NULL;
            }
            return new_addr;
        } else {
            // @TODO: Handle free, resize
            return NULL;
        }
    }

    if (old_virtual_address && size) {
        // RESIZE MEMORY
        
        int found_used_index = -1;
        for (int i = 0; i < g_num_used_regions; i++) {
            Region* alloc = &g_used_regions[i];
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

        Region* used_alloc = &g_used_regions[found_used_index];
        const u64 old_aligned_size = used_alloc->pageCount * PAGE_SIZE;

        void* new_ptr = kernel_alloc(requested_aligned_size, NULL);
        memcpy(new_ptr, old_aligned_ptr, old_aligned_size);
        // TODO: We are doing a loop over used regions once above and then
        //   once below when freeing old allocation. OPTIMIZE.
        //   We'll want to use a different memory allocation implementation
        //   so no point in improving this now.
        kernel_alloc(0, old_aligned_ptr);

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
            Region* alloc = &g_used_regions[i];
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
            Region* alloc = &g_free_regions[i];
            if ((alloc->flags & FLAG_FREE) != 0)
                continue;

            found_free_index = i;
            break;
        }

        if (found_free_index == -1) {
            return NULL;
        }

        Region* free_alloc = &g_free_regions[found_free_index];
        Region* used_alloc = &g_used_regions[found_used_index];

        free_alloc->physicalStart = used_alloc->physicalStart;
        free_alloc->virtualStart  = used_alloc->virtualStart;
        free_alloc->pageCount     = used_alloc->pageCount;
        free_alloc->flags         = FLAG_FREE;

        used_alloc->flags = 0; // clear region

        if (found_free_index >= g_num_free_regions) {
            g_num_free_regions = found_free_index + 1;
        }

        return NULL; // we return NULL on success when freeing

    } else /* if (size) */ {
        // ALLOCATE MEMORY

        int found_free_index = -1;
        for (int i = 0; i < g_num_free_regions; i++) {
            Region* alloc = &g_free_regions[i];
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
            Region* alloc = &g_used_regions[i];
            if ((alloc->flags & FLAG_USED) != 0)
                continue;

            found_used_index = i;
            break;
        }

        if (found_used_index == -1) {
            return NULL;
        }

        Region* free_alloc = &g_free_regions[found_free_index];
        Region* used_alloc = &g_used_regions[found_used_index];

        used_alloc->physicalStart = free_alloc->physicalStart;
        used_alloc->virtualStart  = free_alloc->physicalStart;
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

        void* const new_ptr    = (void*)(used_alloc->virtualStart * PAGE_SIZE);
        const u64 aligned_size = ((size + PAGE_SIZE-1) / PAGE_SIZE) * PAGE_SIZE;

        // Always initialize memory. Malicious program should not be able to read freed memory from other programs.
        memset(new_ptr, 0xDC, aligned_size);

        return new_ptr;
    }
}



static int filter_empty_regions(Region* regions, int count) {
    int begin = 0;
    int end = count-1;
    while (begin <= end) {
        Region* b = &regions[begin];
        Region* e = &regions[end];

        if (b->flags != 0) {
            begin++;
            continue;
        }
        if (e->flags == 0) {
            end--;
            continue;
        }

        *b = *e;
        begin++;
        end--;
    }
    return begin + (regions[begin].flags != 0 ? 1 : 0);
}



static void sort_regions(Region* regions, int left, int right) {
    // quick sort (ChatGPT)
    if (left >= right)
        return;
    
    u64 pivot = regions[(left + right) / 2].physicalStart;
    int i = left;
    int j = right;

    while (i <= j) {
        while (regions[i].physicalStart < pivot)
            i++;
        while (regions[j].physicalStart > pivot)
            j--;
        
        if (i <= j) {
            // maybe some VPXOR x64 AVX2 instruction to speed up swap?
            // It assumes Region is 32 bytes. And not all computers support AVX2.
            Region tmp = regions[i];
            regions[i] = regions[j];
            regions[j] = tmp;
            i++;
            j--;
        }
    }
    if (left < j)
        sort_regions(regions, left, j);
    if (i < right)
        sort_regions(regions, i, right);
}



static void merge_neighbour_regions(Region* regions, int count) {
    int index = 0;
    int read_index = 1;
    while (index < count && read_index < count) {
        Region* b = &regions[index];
        Region* e = &regions[read_index];
        
        if (b->physicalStart + b->pageCount != e->physicalStart) {
            index = read_index;
            read_index++;
            continue;
        }

        b->pageCount += e->pageCount;
        e->flags = 0;
        read_index++;
    }
}



void kernel_compress_regions() {
    // THIS FUNCTION HAS NOT BEEN TESTED
    g_num_free_regions = filter_empty_regions(g_free_regions, g_num_free_regions);

    sort_regions(g_free_regions, 0, g_num_free_regions-1);

    merge_neighbour_regions(g_free_regions, g_num_free_regions);
}


