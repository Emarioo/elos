#include "elos/kernel/audio/hda.h"

#include "elos/kernel_console.h"

#include "elos/physical_memory.h"

#include "elos/common/intrinsics.h"
#include "elos/common/string.h"
#include "elos/cpu.h"

#define printf(...) KCON_printf(__VA_ARGS__)
#define debug(...) printf(__VA_ARGS__)


//#####################
//     TYPES
//#####################


typedef volatile struct {
    u16 GCAP;
    u8  VMIN;
    u8  VMAJ;
    u16 OUTPAY;
    u16 INPAY;
    u32 GCTL;
    u16 WAKEEN;
    u16 STATESTS;
    u16 GSTS;
    u16 _Rsvd0;
    u32 _Rsvd1;
    u16 OUTSTRMPAY;
    u16 INSTRMPAY;
    u32 _Rsvd2;
    u32 INTCTL;
    u32 INTSTS;
    u32 _Rsvd20;
    u32 _Rsvd21;
    u32 WALCLK;
    u32 _Rsvd3;
    u32 SSYNC;
    u32 _Rsvd4;
    // offset 0x40
    u32 CORBLBASE;
    u32 CORBUBASE;
    u16 CORBWP;
    u16 CORBRP;
    u8  CORBCTL;
    u8  CORBSTS;
    u8  CORBSIZE;
    u8  _Rsvd5;
    // offset 0x50
    u32 RIRBLBASE;
    u32 RIRBUBASE;
    u16 RIRBWP;
    u16 RINTCNT;
    u8  RIRBCTL;
    u8  RIRBSTS;
    u8  RIRBSIZE;
    u8  _Rsvd6;
} HDA_Regs;

typedef struct {
    volatile HDA_Regs* regs;

    u32* corbAddress;
    u32* rirbAddress;

    u32   corbWritePointerMask;
    u32   corbWritePointer;
    u32   rirbReadPointerMask;
    u32   rirbReadPointer;
} HDA_Device;

typedef struct {
    u32 verb;
}  HDA_CORB_Entry;

typedef struct {
    u32 verb;
    u32 verb_ext;
}  HDA_RIRB_Entry;


//#####################
//     VARIABLES
//#####################


static u64 ticks_per_sec;


//#####################
//     FUCNTIONS
//#####################


void hda_reset(HDA_Device* dev) {
    volatile HDA_Regs* regs = dev->regs;

    printf("HDA reset\n");
    
    
    regs->GCTL = regs->GCTL & ~1;

    while (dev->regs->GCTL & 1) pause();

    regs->GCTL = regs->GCTL | 1;

    // "Software is responsible for setting/clearing this ibt such that the
    //  minimun link RESET# signal assertion pulse width specification is met".
    // - HDA specification
    //
    // We wait 1ms just to be safe. If we did stuff with the device
    // we may need to reset other things or very that they where reset or something.
    CPU_sleep(1000 * 1000);

    while ((regs->GCTL & 1) == 0) pause();

    // Extra waiting for corb dma engines?
    CPU_sleep(1000 * 1000);

    // @TODO Are we resetting correctly?
    printf("HDA reset, done\n");
}

void hda_dump(HDA_Device* dev) {
    volatile HDA_Regs* regs = dev->regs;
    
    u32 numOutputStreams        = (regs->GCAP >> 12) & 0xF;
    u32 numInputStreams         = (regs->GCAP >> 8)  & 0xF;
    u32 numBidirectionalStreams = (regs->GCAP >> 3)  & 0x1F;
    u32 numSerialDataOutSignals = 1 << ((regs->GCAP >> 1)  & 0x3);
    u32 has64BitAddress         = (regs->GCAP >> 0)  & 0x1;

    printf("High Definition Audio Controller\n");
    printf("  GCAP       0x%x\n", regs->GCAP);
    printf("    Num output streams   %d\n", numOutputStreams);
    printf("    Num onput streams    %d\n", numInputStreams);
    printf("    Num bi-dir. streams  %d\n", numBidirectionalStreams);
    printf("    Num serial d.o.s.    %d\n", numSerialDataOutSignals);
    printf("    64-bit address       %d\n", has64BitAddress);
    printf("  VMIN       0x%x\n", regs->VMIN);
    printf("  VMAJ       0x%x\n", regs->VMAJ);
    printf("  OUTPAY     0x%x\n", regs->OUTPAY);
    printf("  INPAY      0x%x\n", regs->INPAY);
    printf("  GCTL       0x%x\n", regs->GCTL);
    printf("  WAKEEN     0x%x\n", regs->WAKEEN);
    printf("  STATESTS   0x%x\n", regs->STATESTS);
    printf("  GSTS       0x%x\n", regs->GSTS);
    printf("  OUTSTRMPAY 0x%x\n", regs->OUTSTRMPAY);
    printf("  INSTRMPAY  0x%x\n", regs->INSTRMPAY);
    printf("  INTCTL     0x%x\n", regs->INTCTL);
    printf("  INTSTS     0x%x\n", regs->INTSTS);
    printf("  WALCLK     0x%x\n", regs->WALCLK);
    printf("  SSYNC      0x%x\n", regs->SSYNC);
    printf("  CORBLBASE  0x%x\n", regs->CORBLBASE);
    printf("  CORBWP     0x%x\n", regs->CORBWP);
    printf("  CORBRP     0x%x\n", regs->CORBRP);
    printf("  CORBCTL    0x%x\n", regs->CORBCTL);
    printf("  CORBSTS    0x%x\n", regs->CORBSTS);
    printf("  CORBSIZE   0x%x\n", regs->CORBSIZE);
    printf("    mask     0x%x\n", (regs->CORBSIZE >> 4) & 0xF);
    printf("    current  0x%x\n", (regs->CORBSIZE >> 0) & 0x3);
    printf("  RIRBLBASE  0x%x\n", regs->RIRBLBASE);
    printf("  RIRBUBASE  0x%x\n", regs->RIRBUBASE);
    printf("  RIRBWP     0x%x\n", regs->RIRBWP);
    printf("  RINTCNT    0x%x\n", regs->RINTCNT);
    printf("  RIRBCTL    0x%x\n", regs->RIRBCTL);
    printf("  RIRBSTS    0x%x\n", regs->RIRBSTS);
    printf("  RIRBSIZE   0x%x\n", regs->RIRBSIZE);
    printf("    mask     0x%x\n", (regs->RIRBSIZE >> 4) & 0xF);
    printf("    current  0x%x\n", (regs->RIRBSIZE >> 0) & 0x3);
}


void hda_corb_dma_start(HDA_Device* dev) {
    volatile HDA_Regs* regs = dev->regs;
    regs->CORBCTL = regs->CORBCTL | 2;
    while ((regs->CORBCTL & 2) == 0) pause();
}
void hda_corb_dma_stop(HDA_Device* dev) {
    volatile HDA_Regs* regs = dev->regs;
    regs->CORBCTL = regs->CORBCTL & ~2;
    while ((regs->CORBCTL & 2) != 0) pause();
}

void hda_rirb_dma_start(HDA_Device* dev) {
    volatile HDA_Regs* regs = dev->regs;
    regs->RIRBCTL = regs->RIRBCTL | 2;
    while ((regs->RIRBCTL & 2) == 0) pause();
}
void hda_rirb_dma_stop(HDA_Device* dev) {
    volatile HDA_Regs* regs = dev->regs;
    regs->RIRBCTL = regs->RIRBCTL & ~2;
    while ((regs->RIRBCTL & 2) != 0) pause();
}

void hda_cmd_buffers_init(HDA_Device* dev) {
    volatile HDA_Regs* regs = dev->regs;
    dev->corbWritePointer = 0;
    dev->rirbReadPointer = 0;

    // DMA buffers must be stopped. (which they are on reset of course)

    KERNEL_PANIC(((u64)dev->corbAddress & 0x3FF) == 0, "CORB address not 1K aligned");
    KERNEL_PANIC(((u64)dev->corbAddress >> 32) == 0, "CORB address has high 32 bits set");
    regs->CORBLBASE = (u64)dev->corbAddress;
    regs->CORBUBASE = 0;
    regs->CORBWP = regs->CORBWP & ~0xFF;

    regs->CORBRP = regs->CORBRP | (1<<15);
    while ((regs->CORBRP & (1<<15)) == 0) pause();
    regs->CORBRP = regs->CORBRP & ~(1<<15);
    while ((regs->CORBRP & (1<<15))) pause();

    u8 supportedCorbSizes = (regs->CORBSIZE >> 4) & 0xF;

    int chosenCorbSize = -1;
    // Try to pick largest size.
    // 3 is reserved, 2 is 1 KB, 256 entries
    for (int i=2;i>=0;i--) {
        if (supportedCorbSizes & (1 << i)) {
            chosenCorbSize = i;
            break;
        }
    }
    KERNEL_PANIC(chosenCorbSize != -1, "CORBSIZE supports no sizes?");

    if ((regs->CORBSIZE & 3) != chosenCorbSize) {
        regs->CORBSIZE = (regs->CORBSIZE & ~3) | chosenCorbSize;
        KERNEL_PANIC((regs->CORBSIZE & 3) == chosenCorbSize, "CORBSIZE could not be set to suppored size");
    }
    u32 corbSizeMasks[] = {
        2-1,
        16-1,
        256-1,
    };
    dev->corbWritePointerMask = corbSizeMasks[chosenCorbSize];


    KERNEL_PANIC(((u64)dev->rirbAddress & 0x7FF) == 0, "RIRB address not 2K aligned");
    KERNEL_PANIC(((u64)dev->rirbAddress >> 32) == 0, "RIRB address has high 32 bits set");
    regs->RIRBLBASE = (u64)dev->rirbAddress;
    regs->RIRBUBASE = 0;
    regs->RIRBWP = regs->RIRBWP | (1 << 15);

    /*
        https://f.osdev.org/viewtopic.php?t=56072
        HDA problems in Qemu
        "To get the CORB DMA to run, I have to set RINTCNT (BAR0 + 0x5A) to a large value.
        If I leave it at its reset value of 0, the CORB DMA doesn't run at
            all and its read ptr always reads out as 0. "
        - xeyes
    */
    regs->RINTCNT = (regs->RINTCNT & ~0xFF) | 1;

    u8 supportedRirbSizes = (regs->RIRBSIZE >> 4) & 0xF;

    int chosenRirbSize = -1;
    // Try to pick largest size.
    // 3 is reserved, 2 is 1 KB, 256 entries
    for (int i=2;i>=0;i--) {
        if (supportedRirbSizes & (1 << i)) {
            chosenRirbSize = i;
            break;
        }
    }
    KERNEL_PANIC(chosenRirbSize != -1, "RIRBSIZE supports no sizes?");

    if ((regs->RIRBSIZE & 3) != chosenRirbSize) {
        regs->RIRBSIZE = (regs->RIRBSIZE & ~3) | chosenRirbSize;
        KERNEL_PANIC((regs->RIRBSIZE & 3) == chosenRirbSize, "RIRBSIZE could not be set to suppored size");
    }
    u32 rirbSizeMasks[] = {
        2-1,
        16-1,
        256-1,
    };
    dev->rirbReadPointerMask = rirbSizeMasks[chosenRirbSize];

}

bool hda_send_verb(HDA_Device* dev, u32 verb) {
    volatile HDA_Regs* regs = dev->regs;

    u16 wp = regs->CORBWP;
    wp = (wp + 1) & dev->corbWritePointerMask;
    dev->corbAddress[wp] = verb;
    regs->CORBWP = wp;
    dev->corbWritePointer = wp;

    return true;
}
bool hda_receive_response(HDA_Device* dev, HDA_RIRB_Entry* response) {
    volatile HDA_Regs* regs = dev->regs;

    u16 wp = regs->RIRBWP;

    if (wp == dev->rirbReadPointer)
        return false;

    dev->rirbReadPointer = (dev->rirbReadPointer + 1) & dev->rirbReadPointerMask;

    response->verb = dev->rirbAddress[2*dev->rirbReadPointer];
    response->verb_ext = dev->rirbAddress[2*dev->rirbReadPointer + 1];

    return true;
}

bool hda_send_verb_sync(HDA_Device* dev, u32 verb, HDA_RIRB_Entry* response) {
    hda_send_verb(dev, verb);

    u64 timeout = ticks_per_sec;
    u64 startTime = rdtsc();

    while (1) {
        bool yes = hda_receive_response(dev, response);
        if (yes)
            return true;

        u64 nowTime = rdtsc();
        if (nowTime - startTime > timeout)
            break;
        pause();
    }

    return false;
}

static inline u32 hda_make_verb(u8 codec, u8 node, u16 verb, u8 payload) {
    return
        ((u32)codec << 28) |
        ((u32)node  << 20) |
        ((u32)verb  << 8 ) |
        payload;
}

bool hda_scan(ScanInfo* scanInfo, PCI_ConfigSpace* config) {
    bool stopSearching = false;

    ticks_per_sec = CPU_tsc_per_sec();

    printf("PCI vendor=0x%x device=0x%x\n", config->vendorID, config->deviceID);

    u64 barSize = 0;
    void* barAddress = (void*)(u64)(config->header0.bar0 & ~0xf);
    decode_bar_size(config, 0, &barSize);

    bool mapped = PMEM_map_memory(g_kernelPageTable, barAddress, barAddress, barSize, PMEM_FLAG_NOT_CACHED);
    if (!mapped) {
        printf("hda_scan: Could not map 0x%p\n", barAddress);
        goto exit;
    }

    volatile HDA_Regs* regs = (HDA_Regs*)barAddress;
    HDA_Device _dev = {0};
    HDA_Device* dev = &_dev;
    dev->regs = regs;

    dev->corbAddress = PMEM_alloc_phys(PAGE_SIZE, PMEM_FLAG_IDENTITY_MAPPED | PMEM_FLAG_NOT_CACHED);
    if (!dev->corbAddress) {
        printf("hda_scan: Could not allocate CORB memory\n");
        goto exit;
    }
    dev->rirbAddress = PMEM_alloc_phys(PAGE_SIZE, PMEM_FLAG_IDENTITY_MAPPED | PMEM_FLAG_NOT_CACHED);
    if (!dev->rirbAddress) {
        printf("hda_scan: Could not allocate RIRB memory\n");
        goto exit;
    }
    memset(dev->corbAddress, 0x9A, PAGE_SIZE);
    memset(dev->rirbAddress, 0x9A, PAGE_SIZE);

    hda_dump(dev);

    if ((regs->GCTL & 1) == 0) {
        // Controller is in reset state, bring it out of reset.
        hda_reset(dev);
    }


    hda_cmd_buffers_init(dev);

    int codecAddress = -1;
    // 32 is not chosen for any particular reason.
    for (int i=0;i<32;i++) {
        if (regs->STATESTS & (1 << i)) {
            codecAddress = i;
            regs->STATESTS = regs->STATESTS; // clear the event?
            break;
        }
    }

    if (codecAddress == -1) {
        printf("hda_scan: Could not find a codec\n");
        goto exit;
    }

    hda_corb_dma_start(dev);
    hda_rirb_dma_start(dev);

    HDA_RIRB_Entry response = {0};
    u32 verb = hda_make_verb(codecAddress, 0, 0xF00, 0);
    bool got = hda_send_verb_sync(dev, verb, &response);
    printf("Response %d 0x%x 0x%x\n", got, response.verb, response.verb_ext);

    hda_dump(dev);

exit:
    return stopSearching;
}
