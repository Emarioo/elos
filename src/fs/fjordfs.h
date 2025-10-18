/*
    Experimental file system
*/

#include <stdint.h>

typedef struct {
    uint8_t fjord[4];
    uint16_t fs_version;
    uint16_t sector_per_cluster;

} FJORD_boot_sector;