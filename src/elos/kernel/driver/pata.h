

#pragma once

#include "elos/kernel/common/types.h"


void init_pata();


int ata_read_sector(void* buffer, u32 lba);

int ata_write_sector(void* buffer, u32 lba);
