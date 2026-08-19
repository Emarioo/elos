#include "elos/kernel/audio/hda.h"

#include "elos/kernel_console.h"

#include "elos/physical_memory.h"

#include "elos/common/intrinsics.h"
#include "elos/common/string.h"
#include "elos/cpu.h"



#define printf(...) KCON_printf(__VA_ARGS__)
#define debug(...) printf(__VA_ARGS__)



void hda_dump(HDA_Controller* dev) {
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

const char* hda_widget_type_name(int type) {
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
        hda_widget_type_name((caps >> 20) & 0xF));

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


void hda_dump_stream(HDA_Controller* dev, int index) {
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
