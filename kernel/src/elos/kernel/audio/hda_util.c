#include "elos/kernel/audio/hda.h"

#include "elos/kernel_console.h"

#include "elos/physical_memory.h"

#include "elos/common/intrinsics.h"
#include "elos/common/string.h"
#include "elos/cpu.h"

#include "elos/vfs.h"

#include "elos/kernel/audio/wav.h"


#define printf(...) KCON_printf(__VA_ARGS__)
#define debug(...) printf(__VA_ARGS__)


//#####################
//     VARIABLES
//#####################


static u64 ticks_per_sec;


//#####################
//     FUNCTIONS
//#####################


void hda_reset(HDA_Controller* dev) {
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
    CPU_spin_sleep(1000 * 1000);

    while ((regs->GCTL & 1) == 0) pause();

    // Extra waiting for corb dma engines?
    CPU_spin_sleep(1000 * 1000);

    // @TODO Are we resetting correctly?
    printf("HDA reset, done\n");
}

void hda_corb_dma_start(HDA_Controller* dev) {
    volatile HDA_Regs* regs = dev->regs;
    regs->CORBCTL = regs->CORBCTL | 2;
    while ((regs->CORBCTL & 2) == 0) pause();
}
void hda_corb_dma_stop(HDA_Controller* dev) {
    volatile HDA_Regs* regs = dev->regs;
    regs->CORBCTL = regs->CORBCTL & ~2;
    while ((regs->CORBCTL & 2) != 0) pause();
}

void hda_rirb_dma_start(HDA_Controller* dev) {
    volatile HDA_Regs* regs = dev->regs;
    regs->RIRBCTL = regs->RIRBCTL | 2;
    while ((regs->RIRBCTL & 2) == 0) pause();
}
void hda_rirb_dma_stop(HDA_Controller* dev) {
    volatile HDA_Regs* regs = dev->regs;
    regs->RIRBCTL = regs->RIRBCTL & ~2;
    while ((regs->RIRBCTL & 2) != 0) pause();
}


bool hda_send_verb(HDA_Controller* dev, u32 verb) {
    volatile HDA_Regs* regs = dev->regs;

    u16 wp = regs->CORBWP;
    wp = (wp + 1) & dev->corbWritePointerMask;
    dev->corbAddress[wp] = verb;
    regs->CORBWP = wp;
    dev->corbWritePointer = wp;

    // printf(" corb %d %d\n", regs->CORBWP, regs->CORBRP);

    return true;
}
bool hda_receive_response(HDA_Controller* dev, HDA_RIRB_Entry* response) {
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

bool hda_send_verb_sync(HDA_Controller* dev, u32 verb, HDA_RIRB_Entry* response) {
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



u32 hda_make_verb(u8 codec, u8 node, u16 verb, u8 payload) {
    return
        ((u32)codec << 28) |
        ((u32)node  << 20) |
        ((u32)verb  << 8 ) |
        payload;
}

bool hda_get_param(HDA_Controller* dev, u8 codecAddress, u8 node, u16 param, HDA_RIRB_Entry* response) {
    u32 verb = hda_make_verb(codecAddress, node, 0xF00, param);
    return hda_send_verb_sync(dev, verb, response);
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


void hda_reset_stream(HDA_Controller* dev, int index) {
    volatile HDA_StreamDescriptor* stream = &dev->streamDescriptors[index]; // 0 is reserved
    printf("Reset stream %d\n", index);

    stream->CTL_STS = stream->CTL_STS | 1;
    while ((stream->CTL_STS & 1) == 0) pause();

    printf("Latur 0x%x\n", stream->CTL_STS);

    stream->CTL_STS = stream->CTL_STS & ~1;
    while ((stream->CTL_STS & 1) != 0) pause();

    printf("Reset stream %d, done\n", index);
}

void hda_stream_start(HDA_Controller* dev, int index) {
    volatile HDA_StreamDescriptor* stream = &dev->streamDescriptors[index]; // 0 is reserved
    stream->CTL_STS = stream->CTL_STS | 2;
    while ((stream->CTL_STS & 2) == 0) pause();
}
void hda_stream_stop(HDA_Controller* dev, int index) {
    volatile HDA_StreamDescriptor* stream = &dev->streamDescriptors[index]; // 0 is reserved
    stream->CTL_STS = stream->CTL_STS & ~2;
    while ((stream->CTL_STS & 2) != 0) pause();
}


void hda_mute(HDA_Controller* dev, bool enabled) {
    int codecAddress = 0;
    u32 verb;
    HDA_RIRB_Entry response;
    int wi = 2;
    int got;

    GET_PARAM(wi, HDA_PARAM_OUTPUT_AMPLIFIER_CAP);
    int outputAmp = response.verb;

    int gainIndex = gain_index_from_db(outputAmp, 0.0);

    int settings = BIT_SET_OUTPUT_AMP | BIT_SET_LEFT_AMP | BIT_SET_RIGHT_AMP | gainIndex;
    if (enabled)
        settings |= BIT_SET_MUTE;
    SEND(wi, HDA_SET_AMPLIFIER_GAIN_MUTE, settings);
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


void generate_sine(void* buffer, int size, u64* frameOffset) {
    double frequency = 400;
    double amplitude = 0.3;
    const double sampleRate = 48000.0;

    u16* samples = buffer;

    int frames = size / 4; // 16-bit per sound value * 2 channels

    u64 _frameOffset = *frameOffset;
    for (int i = 0; i < frames; i++) {
        int sine_offset = i + _frameOffset;
        double t = (double)(sine_offset) / sampleRate;
        double angle = 2.0 * PI * frequency * t;

        int16_t sample = (int16_t)(32767.0 * amplitude * sin_approx(angle));

        // stereo
        samples[i * 2 + 0] = sample;
        samples[i * 2 + 1] = sample;

        // printf("%d %d\n", i, (int16_t)samples[i*2]);
    }
    *frameOffset += frames;
}

