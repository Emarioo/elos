
#include "elos/kernel/exec/read_elf.h"

#include "vendor/elf.h"

#include "elos/common/string.h"

#include "elos/vfs.h"
#include "elos/physical_memory.h"
#include "elos/kernel_console.h"

#include "elos/kernel/pmem/paging.h"

#include "elos/kernel/config.h"

#include "elos/execution.h"
#include "elos/kernel/driver/acpi.h" // acpi_lapic_address

#include "elos/frame_buffer.h"



#define printf(...) KCON_printf(__VA_ARGS__)

#define debug(...) printf(__VA_ARGS__)
// #define debug(...)

typedef struct {
    const char* path;
    ElfObject*  object;
    u8*         fileData;
    u64         fileSize;
} ParseContext;

bool parse_elf(ParseContext* ctx);

bool read_elf(const char* path, ElfObject* object) {
    bool returnValue = false;
    VFS_Handle handle = VFS_NULL_HANDLE;
    void* fileData = NULL;

    handle = VFS_open(path, VFS_FLAG_READ_ONLY);
    if (!handle) {
        debug("read_elf: Cannot read %s\n", path);
        goto exit;
    }

    VFS_HandleInfo info;
    bool yes = VFS_info(handle, &info);
    if (!yes) {
        debug("read_elf: VFS_info failed %s\n", path);
        goto exit;
    }

    if (info.fileSize <= 0) {
        debug("read_elf: File size of %s is zero\n", path);
        goto exit;
    }
       if (info.fileSize <= 0) {
        debug("read_elf: REFUSE TO READ LARGE ELF %s (%d MB)\n", path, info.fileSize/0x100000);
        goto exit;
    }

    fileData = PMEM_alloc(info.fileSize);

    u64 readBytes = VFS_read(handle, 0, info.fileSize, fileData);
    if (readBytes != info.fileSize) {
        debug("read_elf: Only read %d bytes, not %d\n", info.fileSize);
        goto exit;
    }

    ParseContext context = {
        .path = path,
        .object = object,
        .fileData = fileData,
        .fileSize = info.fileSize,
    };

    bool result = parse_elf(&context);

    returnValue = result;

exit:
    if (handle) {
        VFS_close(handle);
    }
    if (fileData) {
        PMEM_free(fileData);
    }

    return returnValue;
}

#define EHDR_FIELD(V,F) (elfHeader->e_ident[EI_CLASS] == ELFCLASS64 ? ((Elf64_Ehdr*)(V))->F : ((Elf32_Ehdr*)(V))->F )
#define SHDR_FIELD(V,F) (elfHeader->e_ident[EI_CLASS] == ELFCLASS64 ? ((Elf64_Shdr*)(V))->F : ((Elf32_Shdr*)(V))->F )
#define SHDR_INDEX(A,I) (elfHeader->e_ident[EI_CLASS] == ELFCLASS64 ? ((Elf64_Shdr*)(A) + (I)) : (Elf64_Shdr*)((Elf32_Shdr*)(A) + (I)) )
#define RELA_FIELD(V,F) (elfHeader->e_ident[EI_CLASS] == ELFCLASS64 ? ((Elf64_Rela*)(V))->F : ((Elf32_Rela*)(V))->F )

bool parse_elf(ParseContext* ctx) {
    bool returnValue = false;
    Elf64_Ehdr* elfHeader = (Elf64_Ehdr*)ctx->fileData;

    if (memcmp(elfHeader->e_ident, ELFMAG,4)) {
        goto exit;
    }
    if (elfHeader->e_ident[EI_DATA] != ELFDATA2LSB) {
        printf("Bad elf header %s\n", ctx->path);
        goto exit;
    }
    if (elfHeader->e_ident[EI_VERSION] != EV_CURRENT) {
        printf("Bad elf header %s\n", ctx->path);
        goto exit;
    }
    if (elfHeader->e_type != ET_EXEC && elfHeader->e_type != ET_DYN) {
        printf("Bad elf header %s\n", ctx->path);
        goto exit;
    }
    if (elfHeader->e_machine != EM_X86_64 && elfHeader->e_machine != EM_386) {
        printf("Bad elf header %s\n", ctx->path);
        goto exit;
    }

    // printf("etype=%d emach=%d shnum=%d\n", elfHeader->e_type, elfHeader->e_machine, elfHeader->e_shnum);

    Elf64_Shdr* sections = (Elf64_Shdr*)(ctx->fileData + EHDR_FIELD(elfHeader, e_shoff));
    
    Elf64_Shdr* sctionStringTable = SHDR_INDEX(sections, EHDR_FIELD(elfHeader, e_shstrndx));

    char* sectionNames = (char*)(ctx->fileData + SHDR_FIELD(sctionStringTable, sh_offset));

    u64 vaddr_low = -1;
    u64 vaddr_high = 0;
    for (int si = 1; si < EHDR_FIELD(elfHeader, e_shnum); si++) {
        Elf64_Shdr* section = SHDR_INDEX(sections, si);
        const char* name = &sectionNames[SHDR_FIELD(section, sh_name)];

        if (!strcmp(name, ".text")
            || !strcmp(name, ".rodata")
            || !strcmp(name, ".data")
            || !strcmp(name, ".bss")
        ) {
            vaddr_low = min(vaddr_low, SHDR_FIELD(section, sh_addr));
            vaddr_high = max(vaddr_high, SHDR_FIELD(section, sh_addr) + SHDR_FIELD(section, sh_size));
        }
    }

    u64 image_size = vaddr_high;
    // We assume vaddr_low is 0x1000
    // u64 image_size = vaddr_high - vaddr_low;

    #define VADDR_STRIDE 0x1000000

    if (image_size > VADDR_STRIDE) {
        printf("REFUSE TO LOAD 1MB LARGE IMAGE (%d MB)\n", image_size/0x100000);
        goto exit;
    }

    static int elf_count = 0;
    // We give each ELF a different offset so we have a better idea
    // which ELF a page fault address belongs too.
    void* virt_image_base = (void*)(u64)0xC0000000 + elf_count * VADDR_STRIDE;
    void* phys_image_base = PMEM_alloc_phys(image_size, PMEM_FLAG_NONE);
    elf_count++;
    

    PageTable* pageTable = PMEM_allocPageTable();
    
    // Map kernel into user page table (needed when we do syscall)

    // @TODO parse_elf should not be mapping in kernel to user page table...
    PMEM_map_memory(pageTable, __kernel_start, __kernel_start, __kernel_end - __kernel_start, PMEM_FLAG_EXECUTABLE);
    PMEM_map_memory(pageTable, __stack_start, __stack_start, __stack_end - __stack_start, PMEM_FLAG_NONE);
    PMEM_map_memory(pageTable, g_frame_buffer.base, g_frame_buffer.base, g_frame_buffer.size, PMEM_FLAG_NOT_CACHED);
    for (int ci=0;ci<ARRAY_LENGTH(cores);ci++) {
        EXEC_Core* core = &cores[ci];
        u8* stack_start = (u8*)core->syscall_stack - core->syscall_stack_size;
        if (stack_start) {
            // Only map if core has been initialized with a stack?
            PMEM_map_memory(pageTable, stack_start, stack_start, core->syscall_stack_size, PMEM_FLAG_NONE);
        }
    }

    
    PMEM_map_memory(pageTable, (void*)acpi_lapic_address, (void*)acpi_lapic_address, PAGE_SIZE, PMEM_FLAG_NOT_CACHED);

    
    // Map whole image into kernel page tables so we can copy memory from ELF there.
    PMEM_map_memory(g_kernelPageTable, virt_image_base, phys_image_base, image_size, PMEM_FLAG_NONE);

    Elf64_Shdr* relSection = NULL;


    #define MAPPED
    // #define MAPPED printf("ELFLOAD %zx:%zx\n", vaddr, vaddr + section->sh_size);

    // first section is NULL
    for (int si = 1; si < EHDR_FIELD(elfHeader, e_shnum); si++) {
        Elf64_Shdr* section = SHDR_INDEX(sections, si);
        size_t sectionSize = SHDR_FIELD(section, sh_size);

        const char* name = &sectionNames[SHDR_FIELD(section, sh_name)];
        // printf("%s: %d bytes\n", name, sectionSize);

        u8* vaddr = (u8*)virt_image_base + SHDR_FIELD(section, sh_addr);
        u8* paddr = (u8*)phys_image_base + SHDR_FIELD(section, sh_addr);
        u8* src = ctx->fileData + SHDR_FIELD(section, sh_offset);

        // @TODO Check that the virtual addresses for sections don't overlap in pages.
        //    Issues with exec/read only flags otherwise.


        if (!strcmp(name, ".text")) {
            MAPPED
            PMEM_map_memory(pageTable, vaddr, paddr, sectionSize, PMEM_FLAG_USER_SPACE|PMEM_FLAG_READ_ONLY|PMEM_FLAG_EXECUTABLE);
            memcpy(vaddr, src, sectionSize);
        } else if (!strcmp(name, ".rodata")) {
            MAPPED
            PMEM_map_memory(pageTable, vaddr, paddr, sectionSize, PMEM_FLAG_USER_SPACE|PMEM_FLAG_READ_ONLY);
            memcpy(vaddr, src, sectionSize);
        } else if (!strcmp(name, ".data")) {
            MAPPED
            PMEM_map_memory(pageTable, vaddr, paddr, sectionSize, PMEM_FLAG_USER_SPACE);
            memcpy(vaddr, src, sectionSize);
        } else if (!strcmp(name, ".data.rel.ro")) {
            MAPPED
            PMEM_map_memory(pageTable, vaddr, paddr, sectionSize, PMEM_FLAG_USER_SPACE|PMEM_FLAG_READ_ONLY);
            memcpy(vaddr, src, sectionSize);
        } else if (!strcmp(name, ".bss")) {
            MAPPED
            PMEM_map_memory(pageTable, vaddr, paddr, sectionSize, PMEM_FLAG_USER_SPACE);
            memset(vaddr, 0, sectionSize);
        } else if (strstr(name, ".rela")) {
            relSection = section;
        }
    }

    if (relSection) {
        Elf64_Rela* relocations = (Elf64_Rela*)(ctx->fileData + SHDR_FIELD(relSection, sh_offset));
        int relCount = SHDR_FIELD(relSection, sh_size) / SHDR_FIELD(relSection, sh_entsize);
        for (int i=0;i<relCount;i++) {
            Elf64_Rela* rela = (Elf64_Rela*)((char*)relocations + SHDR_FIELD(relSection, sh_entsize) * i);
            int sym;
            int type;

            if (elfHeader->e_ident[EI_CLASS] == ELFCLASS64) {
                sym = ELF64_R_SYM(RELA_FIELD(rela, r_info));
                type = ELF64_R_TYPE(RELA_FIELD(rela, r_info));
            } else {
                sym = ELF32_R_SYM(RELA_FIELD(rela, r_info));
                type = ELF32_R_TYPE(RELA_FIELD(rela, r_info));
            }
            

            if (type == R_X86_64_RELATIVE) {
                u64* pos = (u64*)((u8*)virt_image_base + RELA_FIELD(rela, r_offset));
                *pos = (u64)((u8*)virt_image_base + RELA_FIELD(rela, r_addend));
            } else {
                printf("WARNING: Unhandled relocation sym=%d type=%d addend=%x off=%x\n", sym, type, (int)RELA_FIELD(rela, r_addend), (int)RELA_FIELD(rela, r_offset));
            }
        }
    }


    ctx->object->virt_image_base = virt_image_base;
    ctx->object->phys_image_base = phys_image_base;
    ctx->object->image_size = image_size;
    ctx->object->entry_point = (u8*)virt_image_base + EHDR_FIELD(elfHeader, e_entry);
    ctx->object->pageTable = pageTable;
    ctx->object->compatibilityMode = elfHeader->e_ident[EI_CLASS] == ELFCLASS32;

    returnValue = true;

exit:
    return returnValue;
}
