
#include <stdint.h>

#include "elos/physical_memory.h"


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
    uint64_t physicalStart;
    uint64_t virtualStart; // unused at the moment (same as physical)
    uint64_t pageCount;
    uint32_t flags; // READ,WRITE,EXECUTABLE,MEMORY MAPPED, whether virtualStart is valid
} PhysicalMemoryRegion;

uint32_t g_num_free_regions; // the upper bound, regions beyond this point are all "empty" (without FLAG_FREE,FLAG_USED)
uint32_t g_num_used_regions;
PhysicalMemoryRegion g_free_regions[MAX_REGIONS];
PhysicalMemoryRegion g_used_regions[MAX_REGIONS];


void PMEM_init(BootAPI* boot_api) {
    // g_free_regions = find_free_descriptor(MAX_REGIONS*sizeof(Region));
    // if (!g_free_regions) {
    //     serial_printf("phys: Can't allocate free regions\n");
    //     return false;
    // }
    // serial_printf("phys: Allocated free regions ptr: %x, size: %d\n", g_free_regions, MAX_REGIONS*sizeof(Region));
    
    // g_used_regions = find_free_descriptor(MAX_REGIONS*sizeof(Region));
    // if (!g_used_regions) {
    //     serial_printf("phys: Can't allocate used regions\n");
    //     return false;
    // }
    // serial_printf("phys: Allocated used regions ptr: %x, size: %d\n", g_used_regions, MAX_REGIONS*sizeof(Region));

    // memset(g_free_regions, 0x9D, MAX_REGIONS * sizeof(Region));
    // memset(g_used_regions, 0x9D, MAX_REGIONS * sizeof(Region));

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
