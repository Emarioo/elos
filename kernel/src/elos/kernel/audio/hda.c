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

typedef enum {
    HDA_NODE_NONE,
    HDA_NODE_AUDIO_FUCTION_GROUP,
    HDA_NODE_MODEM_FUCTION_GROUP,
    // This list is incomplete
} HDA_NodeType;

typedef enum {
    HDA_WIDGET_AUDIO_OUTPUT,
    HDA_WIDGET_AUDIO_INPUT,
    HDA_WIDGET_AUDIO_MIXER,
    HDA_WIDGET_AUDIO_SELECTOR,
    HDA_WIDGET_PIN_COMPLEX,
    HDA_WIDGET_POWER_WIDGET,
    HDA_WIDGET_VOLUME_KNOB,
    HDA_WIDGET_BEEP_GENERATOR,
    HDA_WIDGET_VENDOR_DEFINED = 0xF,
} HDA_WidgetType;

typedef enum {
    HDA_GET_PARAMETER                  = 0xF00,
    HDA_GET_CONNECTION_SELECT_CONTROL  = 0xF01,
    HDA_SET_CONNECTION_SELECT_CONTROL  = 0x701,
    HDA_GET_CONNECTION_LIST_ENTRY      = 0xF02,
    HDA_GET_PROCESSING_STATE           = 0xF03,
    HDA_SET_PROCESSING_STATE           = 0x703,
    HDA_GET_COEFFICIENT_INDEX          = 0xD,
    HDA_SET_COEFFICIENT_INDEX          = 0x5,
    HDA_GET_AMPLIFIER_GAIN_MUTE        = 0xB,
    HDA_SET_AMPLIFIER_GAIN_MUTE        = 0x3,
    HDA_GET_CONVERTER_FORMAT           = 0xA,
    HDA_SET_CONVERTER_FORMAT           = 0x2,
    HDA_GET_POWER_STATE                = 0xF05,
    HDA_SET_POWER_STATE                = 0x705,
    HDA_GET_CONVERTER_STREAM_CHANNEL   = 0xF06,
    HDA_SET_CONVERTER_STREAM_CHANNEL   = 0x706,
    HDA_GET_INPUT_CONVERTER_SDI_SELECT = 0xF04,
    HDA_SET_INPUT_CONVERTER_SDI_SELECT = 0x704,
    HDA_GET_PIN_WIDGET_CONTROL         = 0xF07,
    HDA_SET_PIN_WIDGET_CONTROL         = 0x707,
    HDA_GET_UNSOLICITED_RESPONSE       = 0xF08,
    HDA_SET_UNSOLICITED_RESPONSE       = 0x708,
    HDA_GET_PIN_SENSE                  = 0xF09,
    HDA_SET_PIN_SENSE                  = 0x709,
    HDA_GET_EAPD_BTL_ENABLE            = 0xF0C,
    HDA_SET_EAPD_BTL_ENABLE            = 0x70C,
    HDA_GET_VOLUME_KNOB                = 0xF0F,
    HDA_SET_VOLUME_KNOB                = 0x70F,
    HDA_GET_CONFIGURATION_DEFAULT      = 0xF1C,
    HDA_SET1_CONFIGURATION_DEFAULT     = 0x71C,
    HDA_SET2_CONFIGURATION_DEFAULT     = 0x71D,
    HDA_SET3_CONFIGURATION_DEFAULT     = 0x71E,
    HDA_SET4_CONFIGURATION_DEFAULT     = 0x71F,
    // This list is incomplete
} HDA_ControlCommand;

typedef enum {
    HDA_PARAM_VENDOR_ID                = 0x0,
    HDA_PARAM_REVISION_ID              = 0x2,
    HDA_PARAM_SUBORDINATE_NODE_COUNT   = 0x4,
    HDA_PARAM_FUNCTION_GROUP_TYPE      = 0x5,
    HDA_PARAM_AUDIO_FUNCTION_GROUP_CAP = 0x8,
    HDA_PARAM_AUDIO_WIDGET_CAP         = 0x9,
    HDA_PARAM_SUPPORTED_PCM_SIZE_RATES = 0xA,
    HDA_PARAM_SUPPORTED_STREAM_FORMATS = 0xB,
    HDA_PARAM_PIN_CAP                  = 0xC,
    HDA_PARAM_INPUT_AMPLIFIER_CAP      = 0xD,
    HDA_PARAM_OUTPUT_AMPLIFIER_CAP     = 0x12,
    HDA_PARAM_CONNECTION_LIST_LENGTH   = 0xE,
    HDA_PARAM_SUPPORTED_POWER_STATES   = 0xF,
    HDA_PARAM_GPIO_COUT                = 0x11,
    HDA_PARAM_VOLUME_KNOB_CAP          = 0x13,
} HDA_Parameter;

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
    // offset 0x60
    u32 ICOI; // We don't use intermediate command registers
    u32 IRII;
    u16 ICS;
    u16 _Rsvd7;
    u32 _Rsvd8;
    // offset 0x70
    u32 DPLBASE;
    u32 DPUBASE;
} HDA_Regs;


typedef volatile struct {
    // offset 0x80 + 0x20 * N
    u32 CTL_STS;
    u32 LPIB;
    u32 CBL;
    u16 LVI;
    u16 _Rsvd0;
    u16 FIFOS;
    u16 FMT;
    u32 _Rsvd2;
    u32 BDPL;
    u32 BDPU;
} HDA_StreamDescriptor;

const int n = sizeof(HDA_StreamDescriptor);

typedef struct {
    volatile HDA_Regs* regs;
    volatile HDA_StreamDescriptor* streamDescriptors;

    void* barAddress;

    u32* corbAddress;
    u32* rirbAddress;

    u32  corbWritePointerMask;
    u32  corbWritePointer;
    u32  rirbReadPointerMask;
    u32  rirbReadPointer;
    
} HDA_Device;

typedef struct {
    u32 verb;
    u32 verb_ext;
}  HDA_RIRB_Entry;

typedef struct {
    u32 addressLow;
    u32 addressHigh;
    u32 size;
    u32 flags;
} HDA_BufferDescriptor;

//#####################
//     VARIABLES
//#####################


static u64 ticks_per_sec;


//#####################
//     FUNCTIONS
//#####################

void generate_sine(void* buffer, int size);

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
    printf("    Num input streams    %d\n", numInputStreams);
    printf("    Num bi-dir. streams  %d\n", numBidirectionalStreams);
    printf("    Num serial d.o.s.    %d\n", numSerialDataOutSignals);
    printf("    64-bit address       %d\n", has64BitAddress);
    printf("  VMIN       0x%x\n", regs->VMIN);
    printf("  VMAJ       0x%x\n", regs->VMAJ);
    // printf("  OUTPAY     0x%x\n", regs->OUTPAY);
    // printf("  INPAY      0x%x\n", regs->INPAY);
    printf("  GCTL       0x%x\n", regs->GCTL);
    // printf("  WAKEEN     0x%x\n", regs->WAKEEN);
    printf("  STATESTS   0x%x\n", regs->STATESTS);
    printf("  GSTS       0x%x\n", regs->GSTS);
    // printf("  OUTSTRMPAY 0x%x\n", regs->OUTSTRMPAY);
    // printf("  INSTRMPAY  0x%x\n", regs->INSTRMPAY);
    printf("  INTCTL     0x%x\n", regs->INTCTL);
    printf("  INTSTS     0x%x\n", regs->INTSTS);
    printf("  WALCLK     0x%x\n", regs->WALCLK);
    // printf("  SSYNC      0x%x\n", regs->SSYNC);
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
    regs->RIRBCTL = regs->RIRBCTL | 1;

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

// void hda_enable_interrupts(HDA_Device* dev) {
//     volatile HDA_Regs* regs = dev->regs;

//     // global and controller interrupt enable
//     // regs->INTCTL = (1<<31) | (1<<30);

//     // Response interrupt control
//     // @NOCHECKIN Also done when setting up buffers!?
//     regs->RIRBCTL = regs->RIRBCTL | 1;
//     regs->RINTCNT = (regs->RINTCNT & ~0xFF) | 0x1;
// }


bool hda_send_verb(HDA_Device* dev, u32 verb) {
    volatile HDA_Regs* regs = dev->regs;

    u16 wp = regs->CORBWP;
    wp = (wp + 1) & dev->corbWritePointerMask;
    dev->corbAddress[wp] = verb;
    regs->CORBWP = wp;
    dev->corbWritePointer = wp;

    // printf(" corb %d %d\n", regs->CORBWP, regs->CORBRP);

    return true;
}
bool hda_receive_response(HDA_Device* dev, HDA_RIRB_Entry* response) {
    volatile HDA_Regs* regs = dev->regs;

    // @TODO Clear interrupt bit in interrupt handler if we have one.
    //   Otherwise because of QEMU quirk? we must do it here.
    regs->RIRBSTS = 1;

    response->verb = 0;
    response->verb_ext = 0;

    u16 wp = regs->RIRBWP;

    if (wp == dev->rirbReadPointer)
        return false;

    dev->rirbReadPointer = (dev->rirbReadPointer + 1) & dev->rirbReadPointerMask;

    response->verb = dev->rirbAddress[2*dev->rirbReadPointer];
    response->verb_ext = dev->rirbAddress[2*dev->rirbReadPointer + 1];

    // printf(" rirb %d %d\n", regs->RIRBWP, dev->rirbReadPointer);

    return true;
}

bool hda_send_verb_sync(HDA_Device* dev, u32 verb, HDA_RIRB_Entry* response) {
    hda_send_verb(dev, verb);

    u64 timeout = ticks_per_sec; // @TODO One second is a little much?
    u64 startTime = rdtsc();

    while (1) {
        bool yes = hda_receive_response(dev, response);
        if (yes)
            return true;

        u64 nowTime = rdtsc();
        if (nowTime - startTime > timeout) {
            printf("Response timeout\n");
            break;
        }
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

bool hda_get_param(HDA_Device* dev, u8 codecAddress, u8 node, u16 param, HDA_RIRB_Entry* response) {
    u32 verb = hda_make_verb(codecAddress, node, 0xF00, param);
    return hda_send_verb_sync(dev, verb, response);
}



void hda_setup_widgests(HDA_Device* dev);

void hda_stream_buffers(HDA_Device* dev);



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

    HDA_Device _dev = {0};
    HDA_Device* dev = &_dev;
    dev->barAddress = barAddress;
    dev->regs = (HDA_Regs*)barAddress;
    dev->streamDescriptors = (HDA_StreamDescriptor*)((char*)barAddress + 0x80);

    volatile HDA_Regs* regs = (HDA_Regs*)barAddress;

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

    hda_corb_dma_start(dev);
    hda_rirb_dma_start(dev);

    hda_setup_widgests(dev);

    hda_stream_buffers(dev);

    hda_dump(dev);

exit:
    return stopSearching;
}


static const char* widget_type_name(int type) {
    switch (type) {
        case HDA_WIDGET_AUDIO_OUTPUT:   return "Audio Output (DAC)";
        case HDA_WIDGET_AUDIO_INPUT:    return "Audio Input (ADC)";
        case HDA_WIDGET_AUDIO_MIXER:    return "Audio Mixer";
        case HDA_WIDGET_AUDIO_SELECTOR: return "Audio Selector";
        case HDA_WIDGET_PIN_COMPLEX:    return "Pin Complex";
        case HDA_WIDGET_POWER_WIDGET:   return "Power Widget";
        case HDA_WIDGET_VOLUME_KNOB:    return "Volume Knob";
        case HDA_WIDGET_BEEP_GENERATOR: return "Beep Generator";
        case HDA_WIDGET_VENDOR_DEFINED: return "Vendor Defined";
        default:  return "Unknown";
    }
}

void print_widget_caps(u32 caps) {
    printf("Widget Capabilities: 0x%08x\n", caps);

    printf("  Type             : %s\n",
        widget_type_name((caps >> 20) & 0xF));

    printf("  Stereo           : %d\n", (caps >> 0) & 1);
    printf("  Input Amp        : %d\n", (caps >> 1) & 1);
    printf("  Output Amp       : %d\n", (caps >> 2) & 1);
    printf("  Amp Override     : %d\n", (caps >> 3) & 1);
    printf("  Format Override  : %d\n", (caps >> 4) & 1);
    printf("  Stripe           : %d\n", (caps >> 5) & 1);
    printf("  Proc Widget      : %d\n", (caps >> 6) & 1);
    printf("  Unsolicited Resp : %d\n", (caps >> 7) & 1);
    printf("  Connection List  : %d\n", (caps >> 8) & 1);
    printf("  Digital          : %d\n", (caps >> 9) & 1);
    printf("  Power Control    : %d\n", (caps >> 10) & 1);
    printf("  L/R Swap         : %d\n", (caps >> 11) & 1);
    printf("  CP Caps          : %d\n", (caps >> 12) & 1);
    printf("  Delay            : %u samples\n", (caps >> 16) & 0xF);
}

void print_pin_caps(u32 caps)
{
    printf("Pin Capabilities: 0x%08x\n", caps);

    printf("  Impedance Sense : %d\n", (caps >> 0) & 1);
    printf("  Trigger Req     : %d\n", (caps >> 1) & 1);
    printf("  Presence Detect : %d\n", (caps >> 2) & 1);
    printf("  Headphone Drive : %d\n", (caps >> 3) & 1);

    printf("  Output          : %d\n", (caps >> 4) & 1);
    printf("  Input           : %d\n", (caps >> 5) & 1);

    printf("  Balanced I/O    : %d\n", (caps >> 6) & 1);

    printf("  HDMI            : %d\n", (caps >> 7) & 1);

    printf("  VREF Control    : 0x%x\n", (caps >> 8) & 0xFF);

    printf("  EAPD            : %d\n", (caps >> 16) & 1);
    printf("  DisplayPort     : %d\n", (caps >> 24) & 1);
    printf("  High Bit Rate   : %d\n", (caps >> 27) & 1);
}


void print_config_default(u32 cfg) {
        printf("Default Configuration\n");
        printf("  Sequence      : %u\n", (cfg >> 0) & 0xF);
        printf("  Association   : %u\n", (cfg >> 4) & 0xF);
        printf("  Misc          : 0x%x\n", (cfg >> 8) & 0xF);
        printf("  Color         : %u\n", (cfg >> 12) & 0xF);
        printf("  Connection    : %u\n", (cfg >> 16) & 0x3);
        printf("  Device        : %u\n", (cfg >> 20) & 0xF);
        printf("  Location      : %u\n", (cfg >> 24) & 0x3F);
        printf("  Port Conn     : %u\n", (cfg >> 30) & 0x3);
}

void print_supported_pcm(u32 supportedPcm, u32 supportedStreamFormats) {
    printf("Supported PCM/Stream formats: 0x%x 0x%x\n", supportedPcm, supportedStreamFormats);

    printf("  Bit Formats: ");
    int formats[] = {
        8, 16, 20, 24, 32
    };
    for (int i=0;i<5;i++) {
        int bit = (supportedPcm >> (16 + i)) & 1;
        if (bit) {
            printf("%d, ", formats[bit]);
        }
    }
    printf("\n");


    printf("  Sample rates: ");
    const char* rates[] = {
        "8.0",
        "11.025",
        "16.0",
        "22.05",
        "32.0",
        "44.1",
        "48.0",
        "88.2",
        "96.0",
        "176.4",
        "192.0",
        "384.0",
    };
    for (int i=0;i<12;i++) {
        int bit = (supportedPcm >> (i)) & 1;
        if (bit) {
            printf("%s, ", rates[i]);
        }
    }
    printf("\n");
    printf("  PCM:     %d\n", (supportedStreamFormats >> 0) & 1);
    printf("  Float32: %d\n", (supportedStreamFormats >> 1) & 1);
    printf("  AC3:     %d\n", (supportedStreamFormats >> 2) & 1);
}

void print_amp_caps(u32 caps) {
    printf("Output Amplifier Capabilities\n");

    printf("  Offset      : %u\n",  caps        & 0x7F);
    printf("  Num Steps   : %u\n", (caps >> 8)  & 0x7F);
    printf("  Step Size   : %u (0.25 dB units)\n", (caps >> 16) & 0x7F);
    printf("  Mute        : %u\n", (caps >> 31) & 1);
}

int gain_index_from_db(u32 amp_cap, float db_level) {
    int ampOffset   = (amp_cap >> 0)  & 0x7F;
    int ampNumSteps = (amp_cap >> 8)  & 0x7F;
    int ampStepSize = (amp_cap >> 16) & 0x7F;
    int ampMute     = (amp_cap >> 31) & 1;
    int index = db_level / (0.25 * (ampStepSize+1)) + ampOffset;
    if (index < 0)
        index = 0;
    if (index > ampNumSteps)
        index = ampNumSteps;
    return index;
}

void hda_setup_widgests(HDA_Device* dev) {
    volatile HDA_Regs* regs = dev->regs;

    int codecAddress = -1;
    // 32 is not chosen for any particular reason.
    for (int i=0;i<32;i++) {
        if (regs->STATESTS & (1 << i)) {
            codecAddress = i;
            // @NOCHECKIN
            regs->STATESTS = regs->STATESTS; // clear the event?
            break;
        }
    }

    if (codecAddress == -1) {
        printf("hda_scan: Could not find a codec\n");
        goto exit;
    }

    HDA_RIRB_Entry response = {0};
    bool got;
    u32 verb;
    
    #define GET_PARAM(NODE, PARAM) \
        got = hda_get_param(dev, codecAddress, NODE, PARAM, &response); \
        printf("Send nod=%03d param=0x%03x resp=0x%x ext=0x%x\n", NODE, PARAM, response.verb, response.verb_ext);

    #define SEND(NODE, VERB, PAYLOAD) \
        verb = hda_make_verb(codecAddress, NODE, VERB, PAYLOAD); \
        got = hda_send_verb_sync(dev, verb, &response); \
        printf("Send nod=%03d verb=0x%03x payl=0x%03x resp=0x%x ext=0x%x\n", NODE, VERB, PAYLOAD, response.verb, response.verb_ext);

    // vendor id, revision id
    // SEND(0, 0xF00, 0);
    // SEND(0, 0xF00, 2);

    GET_PARAM(0, 4);
    int startNodeNumber = (response.verb >> 16) & 0xFF;
    int nodeCount = (response.verb >> 0) & 0xFF;

    // @TODO Loop other nodes
    GET_PARAM(startNodeNumber, 5);
    int nodeType = response.verb & 0xFF;

    if (nodeType != HDA_NODE_AUDIO_FUCTION_GROUP) {
        printf("hda_scan: Node type is not Audio Function Group\n");
        goto exit;
    }

    GET_PARAM(startNodeNumber, 4);
    int startWidgetNumber = (response.verb >> 16) & 0xFF;
    int widgetCount = (response.verb >> 0) & 0xFF;

    for (int wi=startWidgetNumber;wi<startWidgetNumber+widgetCount;wi++) {
        GET_PARAM(wi, HDA_PARAM_AUDIO_WIDGET_CAP);
        int widgetCap = response.verb;
        GET_PARAM(wi, HDA_PARAM_PIN_CAP);
        int pinCap = response.verb;
        GET_PARAM(wi, HDA_PARAM_INPUT_AMPLIFIER_CAP);
        int inputAmp = response.verb;
        GET_PARAM(wi, HDA_PARAM_OUTPUT_AMPLIFIER_CAP);
        int outputAmp = response.verb;
        GET_PARAM(wi, HDA_PARAM_CONNECTION_LIST_LENGTH);;
        int connectionListLength = response.verb;
        GET_PARAM(wi, HDA_PARAM_VOLUME_KNOB_CAP);
        int volumeKnobCap = response.verb;
        GET_PARAM(wi, HDA_PARAM_SUPPORTED_PCM_SIZE_RATES);
        int supportedPCM = response.verb;
        GET_PARAM(wi, HDA_PARAM_SUPPORTED_STREAM_FORMATS);
        int supportedStreamFormats = response.verb;

        int widgetType = (widgetCap >> 20) & 0xF;
        int hasOutputAmp = (widgetCap >> 2) & 1;
        int hasEABD = (widgetCap >> 16) & 1;

        print_supported_pcm(supportedPCM, supportedStreamFormats);

        print_widget_caps(widgetCap);

        print_pin_caps(pinCap);

        print_amp_caps(outputAmp);

        // Configuration defaults
        SEND(wi, 0xF1C, 0);
        u32 cfg = response.verb;
        print_config_default(cfg);


        int hasConnectionList = (widgetCap >> 8) & 1;
        int longForm = (connectionListLength >> 7) & 1;
        int connLength = (connectionListLength >> 0) & 0x3F;
        
        if (hasConnectionList) {
            printf("Connection list:\n");
            for (int i = 0; i < connLength; ) {

                SEND(wi, 0xF02, i);

                u32 r = response.verb;

                if (longForm) {
                    printf("  -> NID %u\n", r & 0xffff);
                    printf("  -> NID %u\n", (r >> 16) & 0xffff);
                    i += 2;
                } else {
                    for (int j = 0; j < 4 && i < connLength; ++j, ++i) {
                        printf("  -> NID %u\n", (r >> (j * 8)) & 0xff);
                    }
                }
            }
        }

        if (widgetType == HDA_WIDGET_PIN_COMPLEX) {
            // @TODO If there are more than one connection list entries then
            //    We should find out which specific entry we want to set.
            //    In QEMU with hda-output we just have one.
            SEND(wi, HDA_SET_CONNECTION_SELECT_CONTROL, 0);
            
            // enable output pin
            SEND(wi, HDA_SET_PIN_WIDGET_CONTROL, (1<<6));

        } else  if (widgetType == HDA_WIDGET_AUDIO_OUTPUT) {
            if (hasOutputAmp) {
                int gainIndex = gain_index_from_db(outputAmp, 0.0);

                #define BIT_SET_OUTPUT_AMP (1 << 15)
                #define BIT_SET_LEFT_AMP (1 << 13)
                #define BIT_SET_RIGHT_AMP (1 << 12)

                int settings = BIT_SET_OUTPUT_AMP | BIT_SET_LEFT_AMP | BIT_SET_RIGHT_AMP | gainIndex;
                SEND(wi, HDA_SET_AMPLIFIER_GAIN_MUTE, settings);
            }

            if (hasEABD) {
                // set external amplifier to speaker?
                SEND(wi, HDA_SET_EAPD_BTL_ENABLE, (1<<1));
            }


            // @TODO Pick format based on what is supported.
            //    We hardcode a good default for now.

            u32 bits = 0b001; // 16-bits
            u32 channel = 0b1; // 2 channels
            // leaving other fields as zero specifies 48kHz
            u16 payload = (bits << 4) | (channel << 0);
            SEND(wi, HDA_SET_CONVERTER_FORMAT, payload);

            u32 stream = 1; // 0 is reserved for unused
            u32 linkChannel = 0;
            payload = (stream << 4) | (linkChannel << 0);
            SEND(wi, HDA_SET_CONVERTER_STREAM_CHANNEL, payload);
        }
    }

exit:
    return;
}




void hda_dump_stream(HDA_Device* dev, int index) {
    volatile HDA_StreamDescriptor* stream = &dev->streamDescriptors[index]; // 0 is reserved


    printf("Stream %d\n", index);
    printf("  CTL    0x%x\n", (stream->CTL_STS >> 0) & 0xFFFFFF);
    printf("  STS    0x%x\n", (stream->CTL_STS >> 24) & 0xFF);
    printf("  LPIB   0x%x\n", stream->LPIB);
    printf("  CBL    0x%x\n", stream->CBL);
    printf("  LVI    0x%x\n", stream->LVI);
    printf("  FIFOS  0x%x\n", stream->FIFOS);
    printf("  FMT    0x%x\n", stream->FMT);
    printf("  BDPL   0x%x\n", stream->BDPL);
    printf("  BDPU   0x%x\n", stream->BDPU);

}

void hda_reset_stream(HDA_Device* dev, int index) {
    volatile HDA_StreamDescriptor* stream = &dev->streamDescriptors[index]; // 0 is reserved
    printf("Reset stream %d\n", index);

    stream->CTL_STS = stream->CTL_STS | 1;
    while ((stream->CTL_STS & 1) == 0) pause();

    printf("Latur 0x%x\n", stream->CTL_STS);

    stream->CTL_STS = stream->CTL_STS & ~1;
    while ((stream->CTL_STS & 1) != 0) pause();

    printf("Reset stream %d, done\n", index);
}

void hda_stream_start(HDA_Device* dev, int index) {
    volatile HDA_StreamDescriptor* stream = &dev->streamDescriptors[index]; // 0 is reserved
    stream->CTL_STS = stream->CTL_STS | 2;
    while ((stream->CTL_STS & 2) == 0) pause();
}
void hda_stream_stop(HDA_Device* dev, int index) {
    volatile HDA_StreamDescriptor* stream = &dev->streamDescriptors[index]; // 0 is reserved
    stream->CTL_STS = stream->CTL_STS & ~2;
    while ((stream->CTL_STS & 2) != 0) pause();
}


void hda_stream_buffers(HDA_Device* dev) {
    volatile HDA_Regs* regs = dev->regs;

    u32 streamBufferSize = 0x4000;

    int bufferDescriptors_max = PAGE_SIZE/sizeof(HDA_BufferDescriptor);
    int bufferDescriptors_len = 0;
    volatile HDA_BufferDescriptor* bufferDescriptors = PMEM_alloc_phys(PAGE_SIZE, PMEM_FLAG_IDENTITY_MAPPED | PMEM_FLAG_NOT_CACHED);
    memset((HDA_BufferDescriptor*)bufferDescriptors, 0, bufferDescriptors_max * sizeof(*bufferDescriptors));

    void* buffer0 = PMEM_alloc_phys(streamBufferSize, PMEM_FLAG_IDENTITY_MAPPED | PMEM_FLAG_NOT_CACHED);
    void* buffer1 = PMEM_alloc_phys(streamBufferSize, PMEM_FLAG_IDENTITY_MAPPED | PMEM_FLAG_NOT_CACHED);
    KERNEL_PANIC(((u64)buffer0 >> 32) == 0, "High 32 bits of buffer0 are set");
    KERNEL_PANIC(((u64)buffer1 >> 32) == 0, "High 32 bits of buffer1 are set");

    bufferDescriptors[bufferDescriptors_len].addressLow = (u64)buffer0;
    bufferDescriptors[bufferDescriptors_len].addressHigh = (u64)0;
    bufferDescriptors[bufferDescriptors_len].size = streamBufferSize;
    bufferDescriptors_len++;
    bufferDescriptors[bufferDescriptors_len].addressLow = (u64)buffer1;
    bufferDescriptors[bufferDescriptors_len].addressHigh = (u64)0;
    bufferDescriptors[bufferDescriptors_len].size = streamBufferSize;
    bufferDescriptors_len++;

    // @TODO When we submit buffers for reading by codec we need to ensure no data is left in cache.
    //    We may notice these kinds of issues with real hardware, not QEMU.

    u32 numOutputStreams        = (regs->GCAP >> 12) & 0xF;
    u32 numInputStreams         = (regs->GCAP >> 8)  & 0xF;
    u32 numBidirectionalStreams = (regs->GCAP >> 3)  & 0x1F;

    KERNEL_PANIC(numOutputStreams != 0, "HDA has zero output streams");
    int streamIndex = numInputStreams;

    volatile HDA_StreamDescriptor* stream = &dev->streamDescriptors[streamIndex]; // 0 is reserved

    printf("0x%p 0x%p %zx\n", stream ,dev->barAddress, (u64)stream - (u64)dev->barAddress);


    hda_dump_stream(dev, streamIndex);

    hda_stream_stop(dev, streamIndex);

    hda_reset_stream(dev, streamIndex);

    u32 streamNumber = 1;
    u32 ctl = (streamNumber << 20);
    // leave traffic priority, bidirectional control, stripe control and interrupt bits zero and disabled.
    stream->CTL_STS = (stream->CTL_STS & ~0xFF00FFF0) | ctl;

    stream->LVI = (stream->LVI & ~0xFF) | (bufferDescriptors_len - 1);

    u32 bits = 0b001; // 16-bits
    u32 channel = 0b1; // 2 channels
    // leaving other fields as zero specifies 48kHz
    u16 fmt = (bits << 4) | (channel << 0);
    stream->FMT = (stream->FMT & ~0x80) | fmt;


    KERNEL_PANIC(((u64)bufferDescriptors >> 32) == 0, "High 32 bits of bufferDescriptors are set");
    stream->BDPL = (u64)bufferDescriptors;
    stream->BDPU = 0;
    stream->CBL = streamBufferSize * bufferDescriptors_len;
    // @NOCHECKIN This should be bytes right? spec says "CBL must represent an integer number samples."
    //    Bytes must align to an even sample count but the unit is still bytes?

    generate_sine(buffer0, streamBufferSize);
    generate_sine(buffer1, streamBufferSize);

    // printf("Sine done\n");

    hda_stream_start(dev, streamIndex);

    // printf("Dump done\n");

    hda_dump_stream(dev, streamIndex);

    // int prev = stream->LPIB;
    // while (1) {
    //     int now = stream->LPIB;
    //     // if (now != prev) {
    //     //     printf("LPIB %d\n", now);
    //     //     prev = now;
    //     // }
    //     pause();
    // }
    
 }




#define PI 3.14159265358979323846

static double wrap_pi(double x) {
    while (x > PI)
        x -= 2.0 * PI;
    while (x < -PI)
        x += 2.0 * PI;
    return x;
}

static double sin_approx(double x) {
    x = wrap_pi(x);

    double x2 = x * x;

    // x - x^3/3! + x^5/5! - x^7/7! + x^9/9!
    return x
         - x * x2 / 6.0
         + x * x2 * x2 / 120.0
         - x * x2 * x2 * x2 / 5040.0
         + x * x2 * x2 * x2 * x2 / 362880.0;
}


void generate_sine(void* buffer, int size) {
    double frequency = 440;
    double amplitude = 0.3;
    const double sampleRate = 48000.0;

    u16* samples = buffer;

    int frames = size / 4; // 16-bit per sound value * 2 channels

    for (int i = 0; i < frames; i++) {
        double t = (double)i / sampleRate;
        double angle = 2.0 * PI * frequency * t;

        int16_t sample = (int16_t)(32767.0 * amplitude * sin_approx(angle));

        // stereo
        samples[i * 2 + 0] = sample;
        samples[i * 2 + 1] = sample;

        // printf("%d %d\n", i, (int16_t)samples[i*2]);
    }

}

