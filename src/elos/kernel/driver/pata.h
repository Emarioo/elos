

#pragma once

#include "elos/kernel/common/types.h"





void init_pata();


int ata_read_sectors(void* buffer, u64 lba, u64 sectors);

int ata_write_sectors(void* buffer, u64 lba, u64 sectors);

