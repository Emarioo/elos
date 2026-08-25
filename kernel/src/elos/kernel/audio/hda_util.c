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



#define DEFAULT_PCM_16BIT_48KHZ   0x0011
#define DEFAULT_PCM_16BIT_44_1KHZ 0x4011
#define DEFAULT_AUDIO_FORMAT DEFAULT_PCM_16BIT_44_1KHZ



/*
    Example on how to play sound.
*/

void hda_stream_buffers(HDA_Controller* dev) {
    volatile HDA_Regs* regs = dev->regs;



    u32   streamBufferSize;
    void* buffer0;
    void* buffer1;


    WAVFile* wav;
    WAVError err = ReadWAVFile("/PKG/WAV/DREAM.WAV", &wav, true);

    if (err != WAV_SUCCESS) {
        // These values are choosen to divide cleanly and
        // provide smooth looping.
        streamBufferSize = 10 * 4 * (48000/400);
        buffer0 = PMEM_alloc_phys(streamBufferSize, PMEM_FLAG_IDENTITY_MAPPED | PMEM_FLAG_NOT_CACHED);
        buffer1 = PMEM_alloc_phys(streamBufferSize, PMEM_FLAG_IDENTITY_MAPPED | PMEM_FLAG_NOT_CACHED);

        u64 frameOffset = 0;
        generate_sine(buffer0, streamBufferSize, &frameOffset);
        generate_sine(buffer1, streamBufferSize, &frameOffset);
        printf("Sine done\n");
    } else {
        
        // Divide by 4 because 16-bit and 2 channels make up one sample.
        // We don't want to split a sample.
        streamBufferSize = (wav->data_len / 4) * 2;

        buffer0 = PMEM_alloc_phys(streamBufferSize, PMEM_FLAG_IDENTITY_MAPPED | PMEM_FLAG_NOT_CACHED);
        buffer1 = PMEM_alloc_phys(streamBufferSize, PMEM_FLAG_IDENTITY_MAPPED | PMEM_FLAG_NOT_CACHED);

        memcpy(buffer0, wav->data, streamBufferSize);
        memcpy(buffer1, wav->data + streamBufferSize, streamBufferSize);
    }

    KERNEL_PANIC(((u64)buffer0 >> 32) == 0, "High 32 bits of buffer0 are set");
    KERNEL_PANIC(((u64)buffer1 >> 32) == 0, "High 32 bits of buffer1 are set");

    int bufferDescriptors_max = PAGE_SIZE/sizeof(HDA_BufferDescriptor);
    int bufferDescriptors_len = 0;
    volatile HDA_BufferDescriptor* bufferDescriptors = PMEM_alloc_phys(PAGE_SIZE, PMEM_FLAG_IDENTITY_MAPPED | PMEM_FLAG_NOT_CACHED);
    memset((HDA_BufferDescriptor*)bufferDescriptors, 0, bufferDescriptors_max * sizeof(*bufferDescriptors));

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

    int streamIndex;
    if (dev->nextOutputStreamIndex == 0) {
        streamIndex = numInputStreams;
        dev->nextOutputStreamIndex = numInputStreams + 1;
    } else {
        streamIndex = dev->nextOutputStreamIndex;
        dev->nextOutputStreamIndex++;
    }


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

    // u32 bits = 0b001; // 16-bits
    // u32 channel = 0b1; // 2 channels
    // leaving other fields as zero specifies 48kHz
    // u16 fmt = (bits << 4) | (channel << 0);
    u16 fmt = DEFAULT_AUDIO_FORMAT;
    stream->FMT = (stream->FMT & ~0x80) | fmt;


    KERNEL_PANIC(((u64)bufferDescriptors >> 32) == 0, "High 32 bits of bufferDescriptors are set");
    stream->BDPL = (u64)bufferDescriptors;
    stream->BDPU = 0;
    stream->CBL = streamBufferSize * bufferDescriptors_len;
    // @NOCHECKIN This should be bytes right? spec says "CBL must represent an integer number samples."
    //    Bytes must align to an even sample count but the unit is still bytes?




    // hda_mute(dev, true);

    hda_stream_start(dev, streamIndex);

    // printf("Dump done\n");

    // hda_dump_stream(dev, streamIndex);

    // int prev = stream->LPIB;
    u32 timeout = 2000 * ticks_per_sec / 1000;
    int muted = true;
    u64 startTime = rdtsc();
    while (1) {

        // u64 now = rdtsc();
        // if (muted && now - startTime > timeout) {
        //     hda_mute(dev, true);
        //     muted = false;
        // }

        // u32 bufferIndex = (stream->LPIB / streamBufferSize) % bufferDescriptors_len;
        // if (readPointer != bufferIndex) {
        //     readPointer = (readPointer + 1) % bufferDescriptors_len;
        // }
        // int now = stream->LPIB;

        // if (now != prev) {
        //     printf("LPIB %d\n", now);
        //     prev = now;
        // }
        pause();
    }
    
 }

