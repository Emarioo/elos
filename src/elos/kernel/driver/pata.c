
#include "elos/kernel/driver/pata.h"

#include "elos/kernel/log/print.h"
#include "elos/kernel/common/cpu.h"

#include "elos/kernel/common/intrinsics.h"


/*
    This isn't working at the moment. By suspicion is that UEFI does something maybe or somehow disables IO ports or ATA controller?
    Or maybe the primary device has some special thing going on for it. Maybe device1 or secondary device would work better?

    Perhaps AHCI or something else will work?
*/

const int IO_PRIMARY_BASE = 0x1F0;
const int IO_PRIMARY_CONTROL = 0x3F6;
const int device0 = 0xA0;

// Returns non-zero if we didn't get non-bsy
int ata_wait_bsy() {
    int status;
    while (1) {
        status = inb(IO_PRIMARY_BASE + 7);
        if ((status & 0x80) == 0) {
            return 0;
        }
    }
    return status;
}
int ata_wait_drq() {
    int attempts = 5000;
    int status;
    while (1) {
        status = inb(IO_PRIMARY_BASE + 7);
        if ((status & 0x1) != 0) {
            status = inb(IO_PRIMARY_BASE + 1);
            printf("ata_wait_drq: ERR bit in status set, ERR register = %d\n", (int) status);
            break;
        }
        if ((status & 0x8) != 0) {
            // printf("DRQ: Ready to read data, status: %d\n", (int) status);
            return 0;
        }

        if (attempts <= 0) {
            printf("ata_wait_drq: To many attempts, status: %d\n", (int) status);
            return -1;
        }

        attempts--;
    }
    return status;
}

void ata_soft_reset() {
    outb(IO_PRIMARY_CONTROL, 0x4); // SRST
    sleep_ns(5000); // 5 us
    outb(IO_PRIMARY_CONTROL, 0);
}

enum Okay {
    ATADEV_UNKNOWN,
    ATADEV_PATAPI,
    ATADEV_SATAPI,
    ATADEV_PATA,
    ATADEV_SATA,
};

int detect_devtype ()
{
	ata_soft_reset(IO_PRIMARY_CONTROL);		/* waits until master drive is ready again */
	outb(IO_PRIMARY_BASE + 6, 0xA0);
	for (int i=0;i<14;i++)
        inb(IO_PRIMARY_CONTROL);			/* wait 400ns for drive select to work */
	unsigned cl=inb(IO_PRIMARY_BASE + 4);	/* get the "signature bytes" */
	unsigned ch=inb(IO_PRIMARY_BASE + 5);

	/* differentiate ATA, ATAPI, SATA and SATAPI */
	if (cl==0x14 && ch==0xEB) return ATADEV_PATAPI;
	if (cl==0x69 && ch==0x96) return ATADEV_SATAPI;
	if (cl==0 && ch == 0) return ATADEV_PATA;
	if (cl==0x3c && ch==0xc3) return ATADEV_SATA;
	return ATADEV_UNKNOWN;
}

void init_pata() {
    // https://wiki.osdev.org/ATA_PIO_Mode
    printf("Initializing PATA\n");
    
    u16 identify_data[256];

    int dev = detect_devtype();
    printf("Device type %d\n", dev);

    outb(IO_PRIMARY_BASE + 6, device0);
    outb(IO_PRIMARY_BASE + 2, 0); // sector count
    outb(IO_PRIMARY_BASE + 3, 0); // LBA low
    outb(IO_PRIMARY_BASE + 4, 0); // LBA mid
    outb(IO_PRIMARY_BASE + 5, 0); // LBA high
    outb(IO_PRIMARY_BASE + 7, 0xEC); // identify command
    
    u8 status = inb(IO_PRIMARY_BASE + 7);
    
    printf("Status: %d\n", (int)status);
    
    if (!status) {
        printf("No device0 from primary bus\n");
        return;
    }
    
    while (1) {
        status = inb(IO_PRIMARY_BASE + 7);
        if ((status & 0x80) == 0) {
            // Done
            break;
        }
    }
    printf("BSY done, status: %d\n", (int)status);

    u8 lba_mid  = inb(IO_PRIMARY_BASE + 4);
    u8 lba_high = inb(IO_PRIMARY_BASE + 5);
    if (lba_mid | lba_high) {
        printf("NOT ATA, lba mid: %d, high: %d\n", (int)lba_mid, (int)lba_high);
        return;
    }

    while (1) {
        status = inb(IO_PRIMARY_BASE + 7);
        if ((status & 0x1) != 0) {
            status = inb(IO_PRIMARY_BASE + 1);
            printf("ERR bit in status set, ERR register = %d\n", (int) status);
            break;
        }
        if ((status & 0x8) != 0) {
            printf("DRQ: Ready to read data, status: %d\n", (int) status);
            break;
        }
    }

    u16* ptr = (u16*)identify_data;
    int count = 256;
    asm (
        "mov $0, %%eax\n"
        "mov %%eax, %%es\n"
        "rep insw\n"
        : "+D" (ptr), "+c" (count)
        : "d" (IO_PRIMARY_BASE)
        : "memory", "eax"
    );

    printf("48bit mode: %d\n", (int) (identify_data[83] >> 10) & 1);
    printf("28bit max lba: %d\n", (int) *(u32*)&identify_data[60]);
    printf("48bit max lba: %d\n", (int) *(u64*)&identify_data[100]);
}


int ata_read_sectors(void* buffer, u64 lba, u64 sectors) {
    // Assumes device is ready to be read

    int attempts = 0;

start:

    if (attempts > 10) {
        printf("ata_read_sector: Failed %d times\n", (int)attempts);
        return -1;
    }

    int status = ata_wait_bsy();
    if (status) {
        printf("ata_read_sector: Error while BUSY?, status: %d\n", (int)status);
        return -1;
    }
    
    outb(IO_PRIMARY_BASE + 6, 0xE0 | ((lba >> 24) & 0x0F)); // drive + LBA bits 24–27
    outb(IO_PRIMARY_BASE + 1, 0);
    outb(IO_PRIMARY_BASE + 2, 1); // sector count
    outb(IO_PRIMARY_BASE + 3, lba & 0xFF);
    outb(IO_PRIMARY_BASE + 4, (lba >> 8) & 0xFF);
    outb(IO_PRIMARY_BASE + 5, (lba >> 16) & 0xFF);
    outb(IO_PRIMARY_BASE + 7, 0x20); // READ SECTORS command
    
    status = ata_wait_bsy();
    if (status) {
        ata_soft_reset();
        attempts++;
        printf("ata_read_sector: Error while BUSY?, status: %d\n", (int)status);
        goto start;
        return -1;
    }
    status = ata_wait_drq();
    if (status) {
        ata_soft_reset();
        attempts++;
        goto start;
        return -1;
    }

    u16* ptr = (u16*)buffer;
    int count = 256;
    asm (
        "mov $0, %%eax\n"
        "mov %%eax, %%es\n"
        "rep insw\n"
        : "+D" (ptr), "+c" (count)
        : "d" (IO_PRIMARY_BASE)
        : "memory", "eax"
    );
    // for (int i = 0; i < count; i++) {
    //     ptr[i] = inw(IO_PRIMARY_BASE); // 256 words = 512 bytes
    // }

    return 0;
}

int ata_write_sectors(void* buffer, u64 lba, u64 sectors) {
    // Assumes device is ready to be read

    int status = ata_wait_bsy();
    if (status) {
        printf("ata_read_sector: Error while BUSY?, status: %d\n", (int)status);
        return -1;
    }
    
    outb(IO_PRIMARY_BASE + 6, 0xE0 | ((lba >> 24) & 0x0F)); // drive + LBA bits 24–27
    outb(IO_PRIMARY_BASE + 2, 1); // sector count
    outb(IO_PRIMARY_BASE + 3, (u8)lba & 0xFF);
    outb(IO_PRIMARY_BASE + 4, (u8)(lba >> 8) & 0xFF);
    outb(IO_PRIMARY_BASE + 5, (u8)(lba >> 16) & 0xFF);
    outb(IO_PRIMARY_BASE + 7, 0x30); // write SECTORS command
    
    status = ata_wait_bsy();
    if (status) {
        printf("ata_read_sector: Error while BUSY?, status: %d\n", (int)status);
        return -1;
    }
    status = ata_wait_drq();
    if (status)
        return -1;

    u16* ptr = (u16*)buffer;
    for (int i = 0; i < 256; i++) {
        outw(IO_PRIMARY_BASE, ptr[i]); // 256 words = 512 bytes
        sleep_ns(1000);
    }

    outb(IO_PRIMARY_BASE + 7, 0xE7); // flush command

    return 0;
}
