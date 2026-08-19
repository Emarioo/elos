#include "elos/kernel/audio/hda.h"

#include "elos/kernel_console.h"

#include "elos/physical_memory.h"

#include "elos/common/intrinsics.h"
#include "elos/common/string.h"
#include "elos/cpu.h"

#include "elos/vfs.h"

#include "elos/kernel/audio/wav.h"
#include "elos/kernel/audio/audio_internal.h"


#define printf(...) KCON_printf(__VA_ARGS__)
#define debug(...) printf(__VA_ARGS__)

//#####################
//    CONSTANTS
//#####################


#define DEFAULT_PCM_16BIT_48KHZ   0x0011
#define DEFAULT_PCM_16BIT_44_1KHZ 0x4011
#define DEFAULT_AUDIO_FORMAT DEFAULT_PCM_16BIT_44_1KHZ


//#####################
//     VARIABLES
//#####################


static u64 ticks_per_sec;

HDA_Controller g_hdaControllers[10];
const u32 g_hdaControllers_max = ARRAY_LENGTH(g_hdaControllers);
u32 g_hdaControllers_len;


//#####################
//     FUNCTIONS
//#####################


void hda_interrupt(u32 vector, InterruptFrame* frame);


void hda_cmd_buffers_init(HDA_Controller* dev);
void hda_enable_interrupts(HDA_Controller* dev);

void hda_scan_widgets(HDA_Controller* dev);

void hda_stream_buffers(HDA_Controller* dev);


HDA_Controller* reserve_hda_controller() {
    if (g_hdaControllers_len >= g_hdaControllers_max)
        return NULL;
    return &g_hdaControllers[g_hdaControllers_len];
}

bool hda_scan(ScanInfo* scanInfo, PCI_ConfigSpace* config) {
    bool stopSearching = false;

    // Check if we already have a controller for the PCI device.
    // This can't be here but if we do a scan a second and don't find hdaController
    // we should mark it removed. We don't destroy the object. The audio device objects for that HDA can
    // still exist but we mark them as unplugged until you replug (if you ever do). If it never happens
    // then yeah we waste some memory but it's not much. We could free some buffers if we're desperate.

    ticks_per_sec = CPU_ticks_per_second();

    printf("PCI vendor=0x%x device=0x%x\n", config->vendorID, config->deviceID);
    printf(" int_pin=%d int_line=%d\n", config->header0.interrupt_pin, config->header0.interrupt_line);

    u64 barSize = 0;
    void* barAddress = (void*)(u64)(config->header0.bar0 & ~0xf);
    decode_bar_size(config, 0, &barSize);

    bool mapped = PMEM_map_memory(g_kernelPageTable, barAddress, barAddress, barSize, PMEM_FLAG_NOT_CACHED);
    if (!mapped) {
        printf("hda_scan: Could not map 0x%p\n", barAddress);
        goto exit;
    }

    // Maybe do some simple hda querying to make sure it responds before creating controller object.

    HDA_Controller* controller = reserve_hda_controller();

    controller->nextStreamNumber = 1; // 0 is reserved as unused stream (by HDA spec)
    controller->audioDevices_cap = ARRAY_LENGTH(controller->audioDevices);

    controller->barAddress = barAddress;
    controller->regs = (HDA_Regs*)barAddress;
    controller->streamDescriptors = (HDA_StreamDescriptor*)((char*)barAddress + 0x80);

    volatile HDA_Regs* regs = (HDA_Regs*)barAddress;

    controller->corbAddress = PMEM_alloc_phys(PAGE_SIZE, PMEM_FLAG_IDENTITY_MAPPED | PMEM_FLAG_NOT_CACHED);
    if (!controller->corbAddress) {
        printf("hda_scan: Could not allocate CORB memory\n");
        goto exit;
    }
    controller->rirbAddress = PMEM_alloc_phys(PAGE_SIZE, PMEM_FLAG_IDENTITY_MAPPED | PMEM_FLAG_NOT_CACHED);
    if (!controller->rirbAddress) {
        printf("hda_scan: Could not allocate RIRB memory\n");
        goto exit;
    }
    memset(controller->corbAddress, 0x9A, PAGE_SIZE);
    memset(controller->rirbAddress, 0x9A, PAGE_SIZE);

    // hda_dump(controller);

    if ((regs->GCTL & 1) == 0) {
        // Controller is in reset state, bring it out of reset.
        hda_reset(controller);
    }


    hda_cmd_buffers_init(controller);

    hda_corb_dma_start(controller);
    hda_rirb_dma_start(controller);

    hda_enable_interrupts(controller);

    u32 coreIndex = CPU_get_core_index();
    u32 globalIRQ = 0;

    CPU_set_irq(coreIndex, 0, globalIRQ, hda_interrupt);

    hda_scan_widgets(controller);

    int di = 0;
    while (di < controller->audioDevices_len && di < scanInfo->maxCount) {
        AudioDevice_impl* dev = controller->audioDevices[di];
        scanInfo->devices[di] = dev;
        di++;
    }
    scanInfo->count += di;

    // hda_stream_buffers(controller);

    hda_dump(controller);

exit:
    return stopSearching;
}



void hda_cmd_buffers_init(HDA_Controller* dev) {
    volatile HDA_Regs* regs = dev->regs;
    dev->corbWritePointer = 0;
    dev->rirbReadPointer = 0;

    // DMA buffers must be stopped. (which they are on reset of course)
    hda_corb_dma_stop(dev);
    hda_rirb_dma_stop(dev);

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

void hda_enable_interrupts(HDA_Controller* dev) {
    volatile HDA_Regs* regs = dev->regs;

    // global and controller interrupt enable
    // regs->INTCTL = (1<<31) | (1<<30);

    // Response interrupt control
    // @NOCHECKIN Also done when setting up buffers!?
    regs->RIRBCTL = regs->RIRBCTL | 1;
    regs->RINTCNT = (regs->RINTCNT & ~0xFF) | 0x1;
}


void hda_scan_widgets(HDA_Controller* controller) {
    HDA_Controller* dev = controller;
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

            if (controller->audioDevices_len >= controller->audioDevices_cap) {
                // We can't add more devices.
                continue;
            }

            if (hasOutputAmp) {
                int gainIndex = gain_index_from_db(outputAmp, 0.0);


                int settings = BIT_SET_OUTPUT_AMP | BIT_SET_LEFT_AMP | BIT_SET_RIGHT_AMP | gainIndex | BIT_SET_MUTE;
                SEND(wi, HDA_SET_AMPLIFIER_GAIN_MUTE, settings);
            }

            if (hasEABD) {
                // set external amplifier to speaker?
                SEND(wi, HDA_SET_EAPD_BTL_ENABLE, (1<<1));
            }



            // @TODO Pick format based on what is supported.
            //    We hardcode a good default for now.

            // u32 bits = 0b001; // 16-bits
            // u32 channel = 0b1; // 2 channels
            // leaving other fields as zero specifies 48kHz
            // u16 payload = (bits << 4) | (channel << 0);
            u16 payload = DEFAULT_AUDIO_FORMAT;
            SEND(wi, HDA_SET_CONVERTER_FORMAT, payload);

            u32 stream = 1; // 0 is reserved for unused
            u32 linkChannel = 0;
            payload = (stream << 4) | (linkChannel << 0);
            SEND(wi, HDA_SET_CONVERTER_STREAM_CHANNEL, payload);


            AudioDevice_impl* audioDevice = AUDIO_reserve_device();
            audioDevice->type = AUDIO_TYPE_HDA;

            strncpy(audioDevice->audioInfo.name, "speaker", sizeof(audioDevice->audioInfo.name));

            audioDevice->hda.controller = controller;
            audioDevice->hda.streamNumber = controller->nextStreamNumber;
            controller->nextStreamNumber++;

            controller->audioDevices[controller->audioDevices_len] = audioDevice;
            controller->audioDevices_len++;
        }
    }

exit:
    return;
}

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


bool hda_create_buffer(AudioDevice _device, ELOS_AudioFormat* format, u32 bufferSize, ELOS_AudioBuffer** buffer) {
    AudioDevice_impl* device = (AudioDevice_impl*)_device;
    
    HDA_Controller* dev = device->hda.controller;
    volatile HDA_Regs* regs = dev->regs;

    if (bufferSize < 4 * PAGE_SIZE) {
        // For the math below we need at least 2 pages, one for each streamBuffer.
        // We check 4 just to have some extra.
        return false;
    }

    void* audioMemory = PMEM_alloc_phys(bufferSize + PAGE_SIZE, PMEM_FLAG_IDENTITY_MAPPED | PMEM_FLAG_NOT_CACHED);
    
    void* rawAudioBuffer = audioMemory + PAGE_SIZE;
    ELOS_AudioBuffer* audioBufferHeader = (ELOS_AudioBuffer*)((char*)rawAudioBuffer - sizeof(ELOS_AudioBuffer));

    // The buffer should be cache aligned (128 HDA spec says).
    // We use pages for no particular reason other than the memory allocator
    // rounding up to nearest page.

    int numPages = bufferSize / PAGE_SIZE;
    int halfNumPages = numPages / 2;

    // Size should be a multiple of channel*sample

    u32   streamBufferSize0 = halfNumPages * PAGE_SIZE;
    u32   streamBufferSize1 = bufferSize - streamBufferSize0;
    void* buffer0 = rawAudioBuffer;
    void* buffer1 = (char*)rawAudioBuffer + streamBufferSize0;
    
    int bufferDescriptors_max = PAGE_SIZE/sizeof(HDA_BufferDescriptor);
    int bufferDescriptors_len = 0;
    volatile HDA_BufferDescriptor* bufferDescriptors = PMEM_alloc_phys(PAGE_SIZE, PMEM_FLAG_IDENTITY_MAPPED | PMEM_FLAG_NOT_CACHED);
    memset((HDA_BufferDescriptor*)bufferDescriptors, 0, bufferDescriptors_max * sizeof(*bufferDescriptors));

    bufferDescriptors[bufferDescriptors_len].addressLow = (u64)buffer0 & 0xFFFFFFFF;
    bufferDescriptors[bufferDescriptors_len].addressHigh = (u64)buffer0>>32;
    bufferDescriptors[bufferDescriptors_len].size = streamBufferSize0;
    bufferDescriptors_len++;
    bufferDescriptors[bufferDescriptors_len].addressLow = (u64)buffer1 & 0xFFFFFFFF;
    bufferDescriptors[bufferDescriptors_len].addressHigh = (u64)buffer1 >> 32;
    bufferDescriptors[bufferDescriptors_len].size = streamBufferSize1;
    bufferDescriptors_len++;

    // @TODO When we submit buffers for reading by codec we need to ensure no data is left in cache.
    //    We may notice these kinds of issues with real hardware, not QEMU.

    u32 numOutputStreams        = (regs->GCAP >> 12) & 0xF;
    u32 numInputStreams         = (regs->GCAP >> 8)  & 0xF;
    u32 numBidirectionalStreams = (regs->GCAP >> 3)  & 0x1F;

    if (numOutputStreams == 0) {
        // Why do we have zero output streams?
        // @TODO MEMORY LEAK, leaking physical memory allocated above.
        return false;
    }
    
    int streamIndex;
    if (dev->nextOutputStreamIndex == 0) {
        streamIndex = numInputStreams;
        dev->nextOutputStreamIndex = numInputStreams + 1;
    } else {
        streamIndex = dev->nextOutputStreamIndex;
        dev->nextOutputStreamIndex++;
    }
    device->hda.streamIndex = streamIndex;

    volatile HDA_StreamDescriptor* stream = &dev->streamDescriptors[streamIndex]; // 0 is reserved

    // printf("0x%p 0x%p %zx\n", stream ,dev->barAddress, (u64)stream - (u64)dev->barAddress);

    hda_stream_stop(dev, streamIndex);

    hda_reset_stream(dev, streamIndex);

    
    u32 streamNumber = device->hda.streamNumber;
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
    stream->CBL = streamBufferSize0 + streamBufferSize1;
    // @TODO Spec says "CBL must represent an integer number samples."
    //    We need to make sure we don't have odd number of bytes compared to channels*sample_byte_width


    hda_stream_start(dev, streamIndex);


    *buffer = audioBufferHeader;

    

    // hda_mute(dev, true);


    // printf("Dump done\n");

    // hda_dump_stream(dev, streamIndex);

    // int prev = stream->LPIB;
    // u32 timeout = 2000 * ticks_per_sec / 1000;
    // int muted = true;
    // u64 startTime = rdtsc();
    // while (1) {

    //     // u64 now = rdtsc();
    //     // if (muted && now - startTime > timeout) {
    //     //     hda_mute(dev, true);
    //     //     muted = false;
    //     // }

    //     // u32 bufferIndex = (stream->LPIB / streamBufferSize) % bufferDescriptors_len;
    //     // if (readPointer != bufferIndex) {
    //     //     readPointer = (readPointer + 1) % bufferDescriptors_len;
    //     // }
    //     // int now = stream->LPIB;

    //     // if (now != prev) {
    //     //     printf("LPIB %d\n", now);
    //     //     prev = now;
    //     // }
    //     pause();
    // }



    // @TODO Does process have enough memory for maxEntries?

    // @TODO Check if device supports format.

    // KernelAudioBuffer* kernelBuffer = makeAudioBuffer(bufferSize);
    // if (!kernelBuffer) {
    //     goto exit;
    // }

    // int coreIndex = CPU_get_core_index();
    // EXEC_Core* core = &cores[coreIndex];
    // EXEC_Thread* activeThread = &core->threads[core->active_thread];
    // kernelBuffer->thread = activeThread;


    return true;
}


void hda_interrupt(u32 vector, InterruptFrame* frame) {
    // @NOCHECKIN Fix interrupts. Read PCI get interrupt pin/line and so on.
}
