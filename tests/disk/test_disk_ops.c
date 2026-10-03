

#include "trial.h"
#include "string.h"
#include "stdio.h"
#include "elos/elos.h"

void _start() {
    trial_start();

    trial_assert("sample", 1 == 1);

    ELOS_DiskEntry entries[8];
    u32 entryCount = 8;
    u64 cookie = 0;

    elos_disk_enumerate(&cookie, &entryCount, entries);

    for (int i=0;i<entryCount;i++) {
        ELOS_DiskEntry* entry = &entries[i];
        printf("Disk %u, %s, %zu bytes (sectorSize %u)\n", entry->diskID, entry->name, entry->diskSize, entry->sectorSize);
    }

    trial_assert("sample2", 1 == 1);

    trial_end();
}
