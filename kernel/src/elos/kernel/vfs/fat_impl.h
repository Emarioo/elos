
#include "elos/vfs.h"
#include "elos/kernel/vfs/vfile_system.h"

#include "elos/common/string.h"
#include "elos/common/types.h"
#include "elos/physical_memory.h"
#include "elos/cpu.h"
#include "elos/kernel_console.h"

#include "elos/kernel/vfs/gpt.h"
#include "elos/kernel/vfs/mbr.h"
#include "elos/kernel/vfs/fat.h"

typedef u64 FAT_ID;












// IMPLEMENTATION SPECIFIC

typedef struct {
    int fat_version;
    int sectors_per_fat;
    int fat_entries_per_sector; // may not align to a whole sector (because of FAT12)

    int sector_size;

    fat__BPB* bpb;
    fat16__EBPB* ebpb16;
    fat32__EBPB* ebpb32;

    u64 start_lba;
    u64 end_lba;
    DiskDevice device;

    char _bootSector[512];

    char tempSector[512];
    FAT_ID currentDir;
    int current_cluster;
    int entries_index;
    int sector_start;
    int sector_end;
    int sector_index;

} FATContext;

void init_context(FATContext* context, DiskDevice device, u64 start_lba, u64 end_lba);

FAT_ID get_root_directory(FATContext* context);

FAT_ID find_directory(FATContext* context, FAT_ID currentDir, const cstring subname);

// Assumes name doesn't already exist. Use find_directory first.
// @TODO find_directory can be optimized to cache a free entry it finds.
// If subsequently calling create_directory then it use that entry without
// iterating whole directory again.
FAT_ID create_directory(FATContext* context, FAT_ID currentDir, const cstring subname);
bool delete_entry(FATContext* context, FAT_ID currentDir, const cstring subname);


fat__DirectoryEntry* fat_next_entry(FATContext* context, FAT_ID currentDir, int* entryIndex, char* longName);

int get_free_cluster(FATContext* context);

#define FAT_ID_NULL (0)
#define FAT_ID_NON_FAT32_ROOT_DIR (1)

FAT_ID make_fat_id(FATContext* context, int currentCluster);

void extract_fat_id(FATContext* context, FAT_ID fatID, int* cluster, int* sectorStart, int* sectorEnd);


#define fat__FAT12 12
#define fat__FAT16 16
#define fat__FAT32 32

int fat__detect_type(fat__BPB* bpb);

int fat__sane_cstring(const fat__DirectoryEntry* entry, char* out_path);
int fat__cluster_to_sector_offset(FATContext* context, int cluster);


bool fat_mkdir(DiskDevice device, u64 start_lba, u64 end_lba, const cstring path);

VFS_FileObject* find_file_object(DiskDevice device, u64 start_lba, u32 clusterIndex);
extern VFS_FileObject fileObjects[1000];


int fat__get_fat(FATContext* context, int cluster);

u64 read_fat(VFS_Handle_impl* handle, u64 offset, u64 size, void* buffer);

bool fat_remove(DiskDevice device, u64 start_lba, u64 end_lba, const cstring path);
