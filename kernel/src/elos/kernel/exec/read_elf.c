
#include "elos/kernel/exec/read_elf.h"

#include "vendor/elf.h"

#include "elos/common/string.h"

#include "elos/vfs.h"
#include "elos/physical_memory.h"
#include "elos/kernel_console.h"

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

    handle = VFS_open(path, VFS_FLAG_READ);
    if (!handle) {
        debug("read_elf: Cannot read %s\n", path);
        goto exit;
    }

    VFS_HandleInfo info;
    VFS_info(handle, &info);

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


bool parse_elf(ParseContext* ctx) {
    bool returnValue = false;
    Elf64_Ehdr* elfHeader = (Elf64_Ehdr*)ctx->fileData;

    if (memcmp(elfHeader->e_ident, ELFMAG,4)) {
        goto exit;
    }
    if (elfHeader->e_ident[EI_CLASS] != ELFCLASS64) {
        goto exit;
    }
    if (elfHeader->e_ident[EI_DATA] != ELFDATA2LSB) {
        goto exit;
    }
    if (elfHeader->e_ident[EI_VERSION] != EV_CURRENT) {
        goto exit;
    }
    if (elfHeader->e_type != ET_EXEC && elfHeader->e_type != ET_DYN) {
        goto exit;
    }
    if (elfHeader->e_machine != EM_X86_64) {
        goto exit;
    }

    // printf("etype=%d emach=%d shnum=%d\n", elfHeader->e_type, elfHeader->e_machine, elfHeader->e_shnum);

    Elf64_Shdr* sections = (Elf64_Shdr*)(ctx->fileData + elfHeader->e_shoff);
    
    Elf64_Shdr* sctionStringTable = &sections[elfHeader->e_shstrndx];

    char* sectionNames = (char*)(ctx->fileData + sctionStringTable->sh_offset);

    u64 vaddr_low = -1;
    u64 vaddr_high = 0;
    for (int si = 1; si < elfHeader->e_shnum; si++) {
        Elf64_Shdr* section = &sections[si];
        const char* name = &sectionNames[section->sh_name];

        if (!strcmp(name, ".text")
            || !strcmp(name, ".rodata")
            || !strcmp(name, ".data")
            || !strcmp(name, ".bss")
        ) {
            vaddr_low = min(vaddr_low, section->sh_addr);
            vaddr_high = max(vaddr_high, section->sh_addr + section->sh_size);
        }
    }

    u64 image_size = vaddr_high - vaddr_low;

    if (image_size > 0x800000) {
        printf("REFUSE TO LOAD LARGE IMAGE (%d MB)\n", image_size/0x100000);
        goto exit;
    }

    void* virt_image_base = (void*)(u64)0xC0000000;
    void* phys_image_base = PMEM_alloc_phys(image_size, PMEM_FLAG_NONE);
    
    // @TODO Map readonly, executable.
    PMEM_map_memory(virt_image_base, phys_image_base, image_size, PMEM_FLAG_USER_SPACE);

    // first section is NULL
    for (int si = 1; si < elfHeader->e_shnum; si++) {
        Elf64_Shdr* section = &sections[si];

        const char* name = &sectionNames[section->sh_name];
        printf("%s: %d bytes\n", name, section->sh_size);

        if (!strcmp(name, ".text")
            || !strcmp(name, ".rodata")
            || !strcmp(name, ".data")
        ) {
            memcpy((u8*)virt_image_base + section->sh_addr - vaddr_low,
                ctx->fileData + section->sh_offset,
                section->sh_size);
        } else if (!strcmp(name, ".bss")) {
            memset((u8*)virt_image_base + section->sh_addr - vaddr_low,
                0,
                section->sh_size);
        }
    }

    ctx->object->virt_image_base = virt_image_base;
    ctx->object->phys_image_base = phys_image_base;
    ctx->object->image_size = image_size;
    ctx->object->entry_point = (u8*)virt_image_base + elfHeader->e_entry - vaddr_low;

    returnValue = true;

exit:
    return returnValue;
}
