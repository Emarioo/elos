
#include "elos/kernel/vfs/gpt.h"

#include "elos/kernel/vfs/mbr.h"

#include "elos/common/types.h"
#include "elos/common/string.h"

#include "elos/kernel_console.h"


#define printf(...) KCON_printf(__VA_ARGS__)


bool gpt_find_partition(DiskDevice device, int partitionIndex, u64* start_lba, u64* end_lba) {
    bool res;

    u64 sectorSize = 512;

    u8  stackBuffer[2 * 512];
    int buffer_head = 0;
    
    gpt__Header* gptHeader = (gpt__Header*)(stackBuffer + buffer_head);
    buffer_head += sectorSize;

    int gpt_header_lba = 1;
    int gpt_partArray_lba = 2;

    res = DISK_read(device, gpt_header_lba * sectorSize, sectorSize, gptHeader);
    if (!res) {
        return false;
    }


    if (memcmp(gptHeader->signature, "EFI PART", 8)) {
        // NOT Guid Partition Table.

        // @TODO MBR CODE HAS NOT BEEN TESTED.

        mbr__Header* mbrHeader = (mbr__Header*)(gptHeader);

        res = DISK_read(device, 0, sectorSize, mbrHeader);
        if (!res) {
            return false;
        }

        if (mbrHeader->bootSignature != MBR_BOOT_SIGNATURE) {
            // No known partition format.
            return false;
        }

        if (partitionIndex < 0 || partitionIndex >= ARRAY_LENGTH(mbrHeader->partitionRecords)) {
            // out of bounds
            return false;
        }

        mbr__PartitionRecord* mbrPartition = &mbrHeader->partitionRecords[partitionIndex];

        *start_lba = mbrPartition->starting_lba;
        *end_lba = mbrPartition->starting_lba + mbrPartition->size_lba;

        printf("Found MBR Partition (#%d) at LBA %d - %d\n", partitionIndex, *start_lba, *end_lba);
        return true;
    } else {
        gpt__Partition* partitionBlock = (gpt__Partition*)(stackBuffer + buffer_head);
        buffer_head += sectorSize;

        if (gptHeader->entry_size > sectorSize) {
            return false;
        }
        if (partitionIndex < 0 || partitionIndex >= gptHeader->num_entries) {
            return false;
        }

        u64 partitionByteOffset = partitionIndex * gptHeader->entry_size;


        res = DISK_read(device, gpt_partArray_lba * sectorSize + (partitionByteOffset / sectorSize) * sectorSize, sectorSize, partitionBlock);
        if (!res) {
            return false;
        }

        gpt__Partition* partition = &partitionBlock[(partitionByteOffset % sectorSize) / gptHeader->entry_size];

        char partitionName[ARRAY_LENGTH(partition->partition_name) + 1];
        for (int i=0;i<ARRAY_LENGTH(partition->partition_name);i++) {
            u16 chr = partition->partition_name[i];
            partitionName[i] = (char)chr;
            if (chr == 0)
                break;
        }
        partitionName[ARRAY_LENGTH(partition->partition_name)] = '\0';

        *start_lba = partition->start_lba;
        *end_lba = partition->end_lba + 1; // end_lba is inclusive, we deal in exlusive ends

        printf("Found GPT Partition '%s' (#%d) at LBA %d - %d\n", partitionName, partitionIndex, *start_lba, *end_lba);
        return true;
    }
}
