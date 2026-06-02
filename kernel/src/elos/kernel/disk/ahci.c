#include "elos/kernel/disk/ahci.h"

#include "elos/kernel_console.h"
#include "elos/physical_memory.h"

#include "elos/common/string.h"



#define	SATA_SIG_ATA	0x00000101	// SATA drive
#define	SATA_SIG_ATAPI	0xEB140101	// SATAPI drive
#define	SATA_SIG_SEMB	0xC33C0101	// Enclosure management bridge
#define	SATA_SIG_PM	0x96690101	// Port multiplier

#define AHCI_DEV_NULL 0
#define AHCI_DEV_SATA 1
#define AHCI_DEV_SEMB 2
#define AHCI_DEV_PM 3
#define AHCI_DEV_SATAPI 4

#define HBA_PORT_IPM_ACTIVE 1
#define HBA_PORT_DET_PRESENT 3




#define printf(...) KCON_printf(__VA_ARGS__)
#define debug(...) printf(__VA_ARGS__)
// #define debug(...) 


void dump_port(HBA_PORT* port) {
    HBA_FIS* fis = (void*)(u64)port->fb;

    printf("IS=0x%x\n", port->is);
    printf("SERR=0x%x\n", port->serr);
    printf("TFD=0x%x\n", port->tfd);
    printf("CI=0x%x\n", port->ci);
    printf("SACT=0x%x\n", port->sact);
    printf("CMD=0x%x\n", port->cmd);
    printf("SSTS=0x%x\n", port->ssts);
    printf("SIG=0x%x\n", port->sig);
    printf("sdb=0x%x\n", fis->sdbfis);
}

bool ahci_identify(HBA_PORT *port, void* buffer);

DiskDevice_impl* reserve_device(ScanInfo* scanInfo) {
    DiskDevice_impl* device = NULL;
    for (int i=0;i<MAX_DISK_DEVICES;i++) {
        if (impl_diskDevices[i].type == DISK_TYPE_NONE) {
            device = &impl_diskDevices[i];
            break;
        }
    }
    if (!device) {
        printf("[WARNING] Reached Disk Device limit (%d).\n", MAX_DISK_DEVICES);
        return NULL;
    }
    memset(device, 0, sizeof(*device));
    return device;
}

bool ahci_scan(ScanInfo* scanInfo, PCI_ConfigSpace* config) {
    bool stopSearching = false;

    /*
        Read info from PCI config space
        and do some setup of AHCI controller.
    */
    
    // decode_bar(&device->configSpace, NULL, NULL, NULL, NULL);

    // @TODO Verify that device is truly AHCI?

    u32 bar = config->header0.bar5;
    u64 bar_size = 0;
    int head=5;

    // decode_bar_size(&device->configSpace, 5, &bar_size);

    u32 ahci_base = 0;
    if (bar & 0x1) {
        ahci_base = bar & ~0x3;
        // debug("[INFO] bar[%d] IO-mapped addr=%x size=%d KB\n", head, ahci_base, bar_size/1024);
    } else if (((bar >> 1) & 0x6) == 0) {
        ahci_base = bar & ~0xf;
        if (bar & 0x8) {
            // debug("[INFO] bar[%d] 32-bit prefetchable addr=%x size=%d KB\n", head, ahci_base, bar_size/1024);
        } else {
            // debug("[INFO] bar[%d] 32-bit addr=%x size=%d KB\n", head, ahci_base, bar_size/1024);
        }
    } else {
        // printf("Bad BAR5 for disk\n");
        return stopSearching;
    }

    if (!ahci_base) {
        // Note AHCI if bar5 doens't have an address.
        // Handle normal PATA?
        return stopSearching;
    }

    PMEM_map_memory(g_kernelPageTable, (void*)(u64)ahci_base, (void*)(u64)ahci_base, PAGE_SIZE, PMEM_FLAG_NOT_CACHED);

    HBA_MEM* abar = (void*)(u64)ahci_base;

    // printf("Host cap: 0x%x (extended 0x%x)\n", abar->cap, abar->cap2);

    if (abar->cap & AHCI_CAP_SAM) {
        // printf(" only AHCI\n");
    } else {
        abar->ghc = abar->ghc | AHCI_GHC_AE; // Enable AHCI mode
    }

    int version[3] = {
                abar->vs & 0xFF,
        (abar->vs >>  8) & 0xFF,
        (abar->vs >> 16) & 0xFF,
    };

    // printf("AHCI Version %d.%d.%d\n", version[2], version[1], version[0]);

    
    /*
        Find disk devices on the ports of the controller.
    */


    DiskDevice_impl* diskDevices[32];
    int diskDevices_len = 0;

    u32 pi = abar->pi;
    for (int i = 0; i < 32; i++) {
        if (scanInfo->count >= scanInfo->maxCount) {
            // Stop searching, no more room.
            stopSearching = true;
            break;
        }

        if (pi & (1 << i)) {
            int dt = check_type(&abar->ports[i]);
            if (dt == AHCI_DEV_SATA) {
                DiskDevice_impl* dev = reserve_device(scanInfo);
                if (!dev) {
                    // No more room
                    stopSearching = true;
                    break;
                }
                dev->sata.configSpace = *config;
                dev->type = DISK_TYPE_SATA;
                dev->sata.abar = abar;
                dev->sata.portNo = i;
                dev->sata.port = &abar->ports[i];
                diskDevices[diskDevices_len] = dev;
                diskDevices_len++;
                // debug("SATA drive found at port %d\n", i);

            } else if (dt == AHCI_DEV_SATAPI) {
                // debug("SATAPI drive found at port %d\n", i);
            
            } else if (dt == AHCI_DEV_SEMB) {
                // debug("SEMB drive found at port %d\n", i);
            
            } else if (dt == AHCI_DEV_PM) {
                // debug("PM drive found at port %d\n", i);
            
            } else {
                // debug("No drive found at port %d\n", i);
            }
        }
    }
    
    
    /*
        Setup the command list and FIS for the ports.
        (allocate some buffers for DMA)
    */

    for (int i = 0; i < diskDevices_len; i++) {
        DiskDevice_impl* dev = diskDevices[i];
        bool res = port_rebase(dev->sata.port);
        if (!res) {
            dev->type = DISK_TYPE_NONE;
            diskDevices[i] = diskDevices[diskDevices_len - 1];
            diskDevices_len--;
        }
    }

    /*
        Gather basic information about the devices.
        Mainly name and disk size.
    */

    u16 buffer[512/2];
    void* phys_buffer = PMEM_virt_to_phys(g_kernelPageTable, buffer);  // In case buffer isn't identity mapped to physical page.

    for (int i = 0; i < diskDevices_len; i++) {
        DiskDevice_impl* dev = diskDevices[i];

        // Can this command write more than 512 bytes?
        // If it happens we ruin the stack.
        // If we do at the very least we should notice strange stuff
        // when calling this function. A static variable of PMEM_alloc_phys
        // may overwrite some other memory which will be much harder to detect.
        bool res = ahci_identify(dev->sata.port, phys_buffer);
        if (!res) {
            // Device is in wierd state or we can't talk to it with our implementation.
            dev->type = DISK_TYPE_NONE;
            diskDevices[i] = diskDevices[diskDevices_len - 1];
            diskDevices_len--;
            continue;
        }

        u16* my_buffer = phys_buffer;

        #define IS_SIGNIFICANT(C) ((C) != ' ' && (C) != '\t' && (C) != '\n' && (C) != '\r' && (C) != '\f')

        // The model string needs the words byteswapped and
        // the trailing space needs truncating for sanity.
        int lastSignificantCharacter = 0;
        u16* model_id = &my_buffer[27]; // 27-46
        for (int ci = 0; ci < 20; ci++) {
            if (2*ci >= sizeof(dev->diskInfo.name))
                break;
            dev->diskInfo.name[2*ci]   = model_id[ci] >> 8; // Some byte swap action
            
            if (IS_SIGNIFICANT(dev->diskInfo.name[2*ci]))
                lastSignificantCharacter = 2*ci;
            
            if (2*ci+1 >= sizeof(dev->diskInfo.name))
                break;
            dev->diskInfo.name[2*ci+1] = model_id[ci] & 0xFF;

            if (IS_SIGNIFICANT(dev->diskInfo.name[2*ci+1]))
                lastSignificantCharacter = 2*ci+1;
        }
        if (lastSignificantCharacter < sizeof(dev->diskInfo.name)-1)
            dev->diskInfo.name[lastSignificantCharacter+1] = '\0';
        else
            dev->diskInfo.name[sizeof(dev->diskInfo.name)-1] = '\0';

        // Extract some size information
        u64 lba_count = (u64)my_buffer[100] | ((u64)my_buffer[101] << 16) | ((u64)my_buffer[102] << 32) | ((u64)my_buffer[103] << 48);
        u32 words_per_sector = (u32)my_buffer[117] | ((u32)my_buffer[118] << 16);
        // A device does not have to provide words per sector. We assume default 512 bytes per sector if they don't.
        if (words_per_sector == 0)
            words_per_sector = 512/2;

        dev->diskInfo.diskSize = (u64)lba_count * (u64)words_per_sector*2;
        dev->diskInfo.blockSize = words_per_sector*2;

        // printf("Model: %s\n", dev->diskInfo.name);
        // printf("Size: %d MB\n", dev->diskInfo.diskSize >> 20);
        // printf("SectorSize: %d\n", dev->diskInfo.blockSize);
    }


    /*
        Return devices.
    */

    
    for (int i = 0; i < diskDevices_len; i++) {
        DiskDevice_impl* dev = diskDevices[i];

        if (scanInfo->count >= scanInfo->maxCount) {
            // @TODO Free and cleanup PORT.
            dev->type = DISK_TYPE_NONE;
            continue;
        }
        scanInfo->devices[scanInfo->count] = (DiskDevice)dev;
        scanInfo->count++;
    }

    return stopSearching;
}


int probe_port(HBA_MEM *abar)
{
    // Search disk in implemented ports
    uint32_t pi = abar->pi;
    int i = 0;
    while (i<32)
    {
        if (pi & 1)
        {
            int dt = check_type(&abar->ports[i]);
            if (dt == AHCI_DEV_SATA)
            {
                debug("SATA drive found at port %d\n", i);
                return i;
            }
            else if (dt == AHCI_DEV_SATAPI)
            {
                debug("SATAPI drive found at port %d\n", i);
            }
            else if (dt == AHCI_DEV_SEMB)
            {
                debug("SEMB drive found at port %d\n", i);
            }
            else if (dt == AHCI_DEV_PM)
            {
                debug("PM drive found at port %d\n", i);
            }
            else
            {
                debug("No drive found at port %d\n", i);
            }
        }

        pi >>= 1;
        i ++;
    }
    return -1;
}

// Check device type
static int check_type(HBA_PORT *port)
{
    uint32_t ssts = port->ssts;

    uint8_t ipm = (ssts >> 8) & 0x0F;
    uint8_t det = ssts & 0x0F;

    if (det != HBA_PORT_DET_PRESENT)	// Check drive status
        return AHCI_DEV_NULL;
    if (ipm != HBA_PORT_IPM_ACTIVE)
        return AHCI_DEV_NULL;

    switch (port->sig)
    {
    case SATA_SIG_ATAPI:
        return AHCI_DEV_SATAPI;
    case SATA_SIG_SEMB:
        return AHCI_DEV_SEMB;
    case SATA_SIG_PM:
        return AHCI_DEV_PM;
    default:
        return AHCI_DEV_SATA;
    }
}


#define	ACHI_DATA_SIZE	0x100000

#define HBA_PxCMD_ST    0x0001
#define HBA_PxCMD_FRE   0x0010
#define HBA_PxCMD_FR    0x4000
#define HBA_PxCMD_CR    0x8000

bool port_rebase(HBA_PORT *port) {
    stop_cmd(port);	// Stop command engine

    int total_size = 32 * sizeof(HBA_CMD_HEADER)
        + sizeof(HBA_FIS)
        + 32 * sizeof(HBA_CMD_TBL);

    _Static_assert(32 == sizeof(HBA_CMD_HEADER), "Struct bad size");
    _Static_assert(256 == sizeof(HBA_FIS), "Struct bad size");
    _Static_assert(256 == sizeof(HBA_CMD_TBL), "Struct bad size");

    u8* ahci_data = PMEM_alloc_phys(total_size, PMEM_FLAG_IDENTITY_MAPPED|PMEM_FLAG_NOT_CACHED);
    if (!ahci_data) {
        return false;
    }
    memset(ahci_data, 0, total_size);

    // Command list
    port->clb = (u64)(ahci_data);
    port->clbu = 0;

    // FIS
    port->fb = (u64)ahci_data + (32 * sizeof(HBA_CMD_HEADER));
    port->fbu = 0;

    // Command tables
    HBA_CMD_HEADER *cmdheader = (HBA_CMD_HEADER*)(u64)(port->clb);
    for (int i = 0; i < 32; i++) {
        cmdheader[i].prdtl = 8;	// 8 prdt entries per command table
                    // 256 bytes per command table, 64+16+48+16*8
        // Command table offset: 40K + 8K*portno + cmdheader_index*256
        cmdheader[i].ctba = (u64)ahci_data + (32 * sizeof(HBA_CMD_HEADER) + sizeof(HBA_FIS)) + i * sizeof(HBA_CMD_TBL);
        cmdheader[i].ctbau = 0;
    }

    start_cmd(port);	// Start command engine
    return true;
}

// Start command engine
void start_cmd(HBA_PORT *port) {
    // Wait until CR (bit15) is cleared
    while (port->cmd & HBA_PxCMD_CR)
        ;

    // Set FRE (bit4) and ST (bit0)
    port->cmd |= HBA_PxCMD_FRE;
    port->cmd |= HBA_PxCMD_ST;
    port->serr = port->serr;
    port->is = (u32)-1;
}

// Stop command engine
void stop_cmd(HBA_PORT *port) {
    // Clear ST (bit0)
    port->cmd &= ~HBA_PxCMD_ST;

    // Clear FRE (bit4)
    port->cmd &= ~HBA_PxCMD_FRE;

    // Wait until FR (bit14), CR (bit15) are cleared
    while(1)
    {
        if (port->cmd & HBA_PxCMD_FR)
            continue;
        if (port->cmd & HBA_PxCMD_CR)
            continue;
        break;
    }

}

#define HBA_PxIS_TFES (1 << 30)

#define ATA_CMD_IDENTIFY 0xEC
#define ATA_CMD_READ_DMA_EX 0x25
#define ATA_CMD_WRITE_DMA_EX 0x35

#define ATA_DEV_BUSY 0x80
#define ATA_DEV_DRQ 0x08


bool ahci_identify(HBA_PORT *port, void* buffer) {
    port->is = (uint32_t) -1;		// Clear pending interrupt bits

    int slot = find_cmdslot(port);
    if (slot == -1)
        return false;

    HBA_CMD_HEADER *cmdheader = (HBA_CMD_HEADER*)(u64)port->clb;
    cmdheader += slot;
    // cmdheader->cfl = sizeof(FIS_REG_H2D)/sizeof(uint32_t);	// Command FIS size
    cmdheader->w = 0;		// Read from device
    cmdheader->prdtl = 1;	// PRDT entries count

    HBA_CMD_TBL *cmdtbl = (HBA_CMD_TBL*)(u64)(cmdheader->ctba);
    memset(cmdtbl, 0, sizeof(HBA_CMD_TBL));

    memset(&cmdtbl->prdt_entry[0], 0, sizeof(cmdtbl->prdt_entry[0]));
    cmdtbl->prdt_entry[0].dba = (u64) buffer;
    cmdtbl->prdt_entry[0].dbc = 512-1;
    cmdtbl->prdt_entry[0].i = 1;

    // Setup command
    FIS_REG_H2D *cmdfis = (FIS_REG_H2D*)(&cmdtbl->cfis);

    cmdfis->fis_type = FIS_TYPE_REG_H2D;
    cmdfis->c = 1;	// Command
    cmdfis->command = ATA_CMD_IDENTIFY;
    cmdfis->device = 0;

    int spin = 0;

    // The below loop waits until the port is no longer busy before issuing a new command
    while ((port->tfd & (ATA_DEV_BUSY | ATA_DEV_DRQ)) && spin < 1000000)
    {
        spin++;
    }
    if (spin >= 1000000)
    {
        debug("Port is hung\n");
        return false;
    }

    port->ci = 1<<slot;	// Issue command

    // Wait for completion
    while (1)
    {
        // In some longer duration reads, it may be helpful to spin on the DPS bit 
        // in the PxIS port field as well (1 << 5)
        if ((port->ci & (1<<slot)) == 0) 
            break;
        if (port->is & HBA_PxIS_TFES)	// Task file error
        {
            debug("Read disk error, is=0x%x\n", port->is);
            return false;
        }
    }

    // Check again
    if (port->is & HBA_PxIS_TFES)
    {
        debug("Read disk error2, is=0x%x\n", port->is);
        return false;
    }
    

    return true;
}

bool ahci_read(DiskDevice_impl* device, u64 byteOffset, u64 byteSize, void* buffer) {
    HBA_PORT *port = device->sata.port;
    int sectorSize = device->diskInfo.blockSize;

    port->is = (uint32_t) -1;		// Clear pending interrupt bits
    int spin = 0; // Spin lock timeout counter
    int slot = find_cmdslot(port);
    if (slot == -1)
        return false;

    u64 offset = byteOffset/sectorSize;
    u64 size = byteSize/sectorSize;

    // HBA_FIS* fis = (void*)(u64)port->fb;

    HBA_CMD_HEADER *cmdheader = (HBA_CMD_HEADER*)(u64)port->clb;
    cmdheader += slot;
    cmdheader->cfl = sizeof(FIS_REG_H2D)/sizeof(uint32_t);	// Command FIS size
    cmdheader->w = 0;		// Read from device
    // cmdheader->prdtl = (uint16_t)((count-1)>>4) + 1;	// PRDT entries count
    cmdheader->prdtl = 1;	// PRDT entries count

    HBA_CMD_TBL *cmdtbl = (HBA_CMD_TBL*)(u64)(cmdheader->ctba);
    memset(cmdtbl, 0, sizeof(HBA_CMD_TBL));
    memset(&cmdtbl->prdt_entry[0], 0, sizeof(cmdtbl->prdt_entry[0]));

    if (byteSize >= 0x400000) {
        printf("ahci_read: Cannot read so many bytes, %d\n", byteSize);
        return false;
    }

    cmdtbl->prdt_entry[0].dba = (u64)buffer & 0xFFFFFFFF;
    cmdtbl->prdt_entry[0].dbau = (u64)buffer >> 32;
    cmdtbl->prdt_entry[0].dbc = byteSize;
    // cmdtbl->prdt_entry[0].i = 1;

    // Setup command
    FIS_REG_H2D *cmdfis = (FIS_REG_H2D*)(&cmdtbl->cfis);

    cmdfis->fis_type = FIS_TYPE_REG_H2D;
    cmdfis->c = 1;	// Command
    cmdfis->command = ATA_CMD_READ_DMA_EX;

    cmdfis->lba0 = (uint8_t)offset;
    cmdfis->lba1 = (uint8_t)(offset>>8);
    cmdfis->lba2 = (uint8_t)(offset>>16);
    cmdfis->device = 1<<6;	// LBA mode

    cmdfis->lba3 = (uint8_t)(offset>>24);
    cmdfis->lba4 = (uint8_t)(offset>>32);
    cmdfis->lba5 = (uint8_t)(offset>>40);

    cmdfis->countl = (size) & 0xFF;
    cmdfis->counth = ((size) >> 8) & 0xFF;

    // The below loop waits until the port is no longer busy before issuing a new command
    while ((port->tfd & (ATA_DEV_BUSY | ATA_DEV_DRQ)) && spin < 1000000)
    {
        spin++;
    }
    if (spin == 1000000)
    {
        debug("Port is hung\n");
        return false;
    }

    // debug("port.is=0x%x\n", port->is);

    
    port->ci = 1<<slot;	// Issue command

    // dump_port(port);

    // Wait for completion
    while (1)
    {
        // In some longer duration reads, it may be helpful to spin on the DPS bit 
        // in the PxIS port field as well (1 << 5)
        if ((port->ci & (1<<slot)) == 0) 
            break;
        if (port->is & HBA_PxIS_TFES)	// Task file error
        {
            debug("Read disk error, is=0x%x\n", port->is);
            

            // printf("FIS status=0x%x error=0x%x\n", );
            return false;
        }
    }

    // Check again
    if (port->is & HBA_PxIS_TFES)
    {
        debug("Read disk error2, is=0x%x\n", port->is);
        return false;
    }

    return true;
}


bool ahci_write(DiskDevice_impl* device, u64 byteOffset, u64 byteSize, void* buffer) {
    HBA_PORT *port = device->sata.port;
    int sectorSize = device->diskInfo.blockSize;

    port->is = (uint32_t) -1;		// Clear pending interrupt bits
    int spin = 0; // Spin lock timeout counter
    int slot = find_cmdslot(port);
    if (slot == -1)
        return false;

    u64 offset = byteOffset/sectorSize;
    u64 size = byteSize/sectorSize;

    // HBA_FIS* fis = (void*)(u64)port->fb;

    HBA_CMD_HEADER *cmdheader = (HBA_CMD_HEADER*)(u64)port->clb;
    cmdheader += slot;
    cmdheader->cfl = sizeof(FIS_REG_H2D)/sizeof(uint32_t);	// Command FIS size
    cmdheader->w = 0;		// Read from device
    // cmdheader->prdtl = (uint16_t)((count-1)>>4) + 1;	// PRDT entries count
    cmdheader->prdtl = 1;	// PRDT entries count

    HBA_CMD_TBL *cmdtbl = (HBA_CMD_TBL*)(u64)(cmdheader->ctba);
    memset(cmdtbl, 0, sizeof(HBA_CMD_TBL));
    memset(&cmdtbl->prdt_entry[0], 0, sizeof(cmdtbl->prdt_entry[0]));

    if (byteSize >= 0x400000) {
        printf("ahci_read: Cannot read so many bytes, %d\n", byteSize);
        return false;
    }

    cmdtbl->prdt_entry[0].dba = (u64)buffer & 0xFFFFFFFF;
    cmdtbl->prdt_entry[0].dbau = (u64)buffer >> 32;
    cmdtbl->prdt_entry[0].dbc = byteSize;
    // cmdtbl->prdt_entry[0].i = 1;

    // Setup command
    FIS_REG_H2D *cmdfis = (FIS_REG_H2D*)(&cmdtbl->cfis);

    cmdfis->fis_type = FIS_TYPE_REG_H2D;
    cmdfis->c = 1;	// Command
    cmdfis->command = ATA_CMD_WRITE_DMA_EX;

    cmdfis->lba0 = (uint8_t)offset;
    cmdfis->lba1 = (uint8_t)(offset>>8);
    cmdfis->lba2 = (uint8_t)(offset>>16);
    cmdfis->device = 1<<6;	// LBA mode

    cmdfis->lba3 = (uint8_t)(offset>>24);
    cmdfis->lba4 = (uint8_t)(offset>>32);
    cmdfis->lba5 = (uint8_t)(offset>>40);

    cmdfis->countl = (size) & 0xFF;
    cmdfis->counth = ((size) >> 8) & 0xFF;

    // The below loop waits until the port is no longer busy before issuing a new command
    while ((port->tfd & (ATA_DEV_BUSY | ATA_DEV_DRQ)) && spin < 1000000)
    {
        spin++;
    }
    if (spin == 1000000)
    {
        debug("Port is hung\n");
        return false;
    }
    
    port->ci = 1<<slot;	// Issue command

    // Wait for completion
    while (1)
    {
        // In some longer duration reads, it may be helpful to spin on the DPS bit 
        // in the PxIS port field as well (1 << 5)
        if ((port->ci & (1<<slot)) == 0) 
            break;
        if (port->is & HBA_PxIS_TFES)	// Task file error
        {
            debug("Read disk error, is=0x%x\n", port->is);
            
            return false;
        }
    }

    // Check again
    if (port->is & HBA_PxIS_TFES)
    {
        debug("Read disk error2, is=0x%x\n", port->is);
        return false;
    }

    return true;
}

// Find a free command list slot
int find_cmdslot(HBA_PORT *port)
{
    // If not set in SACT and CI, the slot is free
    uint32_t slots = (port->sact | port->ci);
    for (int i=0; i<32; i++)
    {
        if ((slots&1) == 0)
            return i;
        slots >>= 1;
    }
    debug("Cannot find free command list entry\n");
    return -1;
}
