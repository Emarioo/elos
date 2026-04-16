/*
    Memory allocator
*/

#pragma once

#include "elos/kernel/common/types.h"

#include <efi.h>


typedef struct MemoryMapper {
    EFI_MEMORY_DESCRIPTOR* memory_descriptors;
    UINTN descriptor_size;
    UINTN total_size_of_descriptors;
    UINTN map_key;
    UINT32 desc_version;


} MemoryMapper;

extern MemoryMapper g_memory_mapper;



bool kernel_init_memory_mapper();

void kernel_compress_regions();

void* kernel_alloc(u64 bytes, void* ptr);

/*
    Flags specify write/read/execute access and other flags (zero initialized or 0x9D initialized)
    Address and bytes must be page-aligned
*/
bool kernel_vmap(void* requested_virtual_addr, u64 bytes, int flags);

/*
    Address and bytes must be page-aligned
*/
bool kernel_vunmap(void* requested_virtual_addr, u64 bytes);


/*
    TODO: Allocates contiguous pages with some optional flags
    
        Option to choose how memory is initialized. Zero initialized or some specific byte like 0xCD
        (allocated pages are always initialized, we don't want to leave leftover data which a malicious program read)
        DEBUG boundaries. Allocating 2 extra pages at start and end where page is filled with 0xCD. If they were modified
        we'll know and can notify the user. (user might call kernel_allocated_pages_wrote_to_debug_page() to check it)
        READ,WRITE,EXECUTE permissions. Well, you would maybe start with READ,WRITE,EXEC but then using kernel_set_flags
        turn off write permissions or something.


*/
void* kerneL_alloc_pages(u64 size, void* ptr, u32 flags);


/*
    Does no memory mapping
    Memory is uninitialized
*/
void* kerneL_alloc_phys_pages(u64 requested_pages);
