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
    printf("sdb=0x%x 0x%x\n", fis->sdbfis[0], fis->sdbfis[1]);
}

bool ahci_identify(HBA_PORT *port, void* buffer);

void ahci_init(DiskDevice_impl* device) {

    // decode_bar(&device->configSpace, NULL, NULL, NULL, NULL);


    u32 bar = device->configSpace.header0.bar5;
    u64 bar_size = 0;
    int head=5;


    decode_bar_size(&device->configSpace, 5, &bar_size);

    u32 ahci_base = 0;
    if (bar & 0x1) {
        ahci_base = bar & ~0x3;
        debug("[INFO] bar[%d] IO-mapped addr=%x size=%d KB\n", head, ahci_base, bar_size/1024);
    } else if (((bar >> 1) & 0x6) == 0) {
        ahci_base = bar & ~0xf;
        if (bar & 0x8) {
            debug("[INFO] bar[%d] 32-bit prefetchable addr=%x size=%d KB\n", head, ahci_base, bar_size/1024);
        } else {
            debug("[INFO] bar[%d] 32-bit addr=%x size=%d KB\n", head, ahci_base, bar_size/1024);
        }
    } else {
        printf("Bad BAR5 for disk\n");
        return;
    }

    PMEM_map_memory((void*)(u64)ahci_base, (void*)(u64)ahci_base, PAGE_SIZE, PMEM_FLAG_NOT_CACHED);

    HBA_MEM* abar = (void*)(u64)ahci_base;

    printf("Host cap: 0x%x (extended 0x%x)\n", abar->cap, abar->cap2);

    if (abar->cap & AHCI_CAP_SAM) {
        printf(" only AHCI\n");
    } else {
        abar->ghc = abar->ghc | AHCI_GHC_AE; // Enable AHCI mode
    }

    int version[3] = {
                abar->vs & 0xFF,
        (abar->vs >>  8) & 0xFF,
        (abar->vs >> 16) & 0xFF,
    };

    printf("AHCI Version %d.%d.%d\n", version[2], version[1], version[0]);

    
    // printf("BIOS handoff status: 0x%x\n", abar->bohc);

    int portNo = probe_port(abar);
    if (portNo == -1) {
        printf("Did not find a AHCI port\n");
        return;
    }

    HBA_PORT* port = &abar->ports[portNo];


    port_rebase(port, portNo);

    // dump_port(port);

    static char* buffer;

    buffer = PMEM_alloc(0x10000);

    bool yes = false;

    yes = ahci_identify(port, buffer);

    u16* id = (u16*)buffer + 27;
    printf("Model: ");
    int count = 0;
    while (count < 20) {
        printf("%c%c", (char)(id[count] >> 8), id[count]&0xFF);
        count++;
    }
    printf("\n");
    u32 lba_count = *(u32*)((u16*)buffer + 100);
    printf("lba_count: %d\n", lba_count);

    // dump_port(port);

    yes = ahci_read(port, 0, 1, buffer);
    
    printf("Data at sector 0: ");
    for (int i=0;i<16;i++) {
        printf("%c", buffer[i]);
    }
    printf("\n");
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

void port_rebase(HBA_PORT *port, int portno)
{
	stop_cmd(port);	// Stop command engine


    u8* ahci_data = PMEM_alloc_phys(ACHI_DATA_SIZE, PMEM_FLAG_IDENTITY_MAPPED|PMEM_FLAG_NOT_CACHED);

    memset(ahci_data, 0, ACHI_DATA_SIZE);

	// Command list offset: 1K*portno
	// Command list entry size = 32
	// Command list entry maxim count = 32
	// Command list maxim size = 32*32 = 1K per port
	port->clb = (u64)(ahci_data + (portno<<10));
	port->clbu = 0;
	memset((void*)(u64)(port->clb), 0, 1024);

	// FIS offset: 32K+256*portno
	// FIS entry size = 256 bytes per port
	port->fb = (u64)ahci_data + (32<<10) + (portno<<8);
	port->fbu = 0;
	memset((void*)(u64)(port->fb), 0, 256);

	// Command table offset: 40K + 8K*portno
	// Command table size = 256*32 = 8K per port
	HBA_CMD_HEADER *cmdheader = (HBA_CMD_HEADER*)(u64)(port->clb);
	for (int i=0; i<32; i++)
	{
		cmdheader[i].prdtl = 8;	// 8 prdt entries per command table
					// 256 bytes per command table, 64+16+48+16*8
		// Command table offset: 40K + 8K*portno + cmdheader_index*256
		cmdheader[i].ctba = (u64)ahci_data + (40<<10) + (portno<<13) + (i<<8);
		cmdheader[i].ctbau = 0;
		memset((void*)(u64)cmdheader[i].ctba, 0, 256);
	}

	start_cmd(port);	// Start command engine
}

// Start command engine
void start_cmd(HBA_PORT *port)
{
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
void stop_cmd(HBA_PORT *port)
{
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

    // debug("port.is=0x%x\n", port->is);

    // port->sact = port->sact | (1<<slot);



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
    
    // printf("0x0: ");
    // for (int i=0;i<16;i++) {
    //     printf("%2x ", (u32)(u8)buffer[i]);
    // }

	return true;
}

bool ahci_read(HBA_PORT *port, u64 start, u16 count, void *buf)
{
	port->is = (uint32_t) -1;		// Clear pending interrupt bits
	int spin = 0; // Spin lock timeout counter
	int slot = find_cmdslot(port);
	if (slot == -1)
		return false;

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

	cmdtbl->prdt_entry[0].dba = (u64) buf;
	cmdtbl->prdt_entry[0].dbc = 512;	// 512 bytes per sector
	cmdtbl->prdt_entry[0].i = 1;

	// Setup command
	FIS_REG_H2D *cmdfis = (FIS_REG_H2D*)(&cmdtbl->cfis);

	cmdfis->fis_type = FIS_TYPE_REG_H2D;
	cmdfis->c = 1;	// Command
	cmdfis->command = ATA_CMD_READ_DMA_EX;

	cmdfis->lba0 = (uint8_t)start;
	cmdfis->lba1 = (uint8_t)(start>>8);
	cmdfis->lba2 = (uint8_t)(start>>16);
	cmdfis->device = 1<<6;	// LBA mode

	cmdfis->lba3 = (uint8_t)(start>>24);
	cmdfis->lba4 = (uint8_t)(start>>32);
	cmdfis->lba5 = (uint8_t)(start>>40);

	cmdfis->countl = count & 0xFF;
	cmdfis->counth = (count >> 8) & 0xFF;

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
