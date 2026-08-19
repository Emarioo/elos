#pragma once

#include "elos/common/types.h"

typedef struct ScanInfo ScanInfo;
typedef struct AudioDevice_impl AudioDevice_impl;

#include "elos/syscalls.h"
#include "elos/kernel/driver/pci.h"
#include "elos/kernel/driver/pci_list.h"

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


#define BIT_SET_OUTPUT_AMP (1 << 15)
#define BIT_SET_LEFT_AMP (1 << 13)
#define BIT_SET_RIGHT_AMP (1 << 12)
#define BIT_SET_MUTE (1 << 7)


#define GET_PARAM(NODE, PARAM) \
    got = hda_get_param(dev, codecAddress, NODE, PARAM, &response); \
    printf("Send nod=%03d param=0x%03x resp=0x%x ext=0x%x\n", NODE, PARAM, response.verb, response.verb_ext);

#define SEND(NODE, VERB, PAYLOAD) \
    verb = hda_make_verb(codecAddress, NODE, VERB, PAYLOAD); \
    got = hda_send_verb_sync(dev, verb, &response); \
    printf("Send nod=%03d verb=0x%03x payl=0x%03x resp=0x%x ext=0x%x\n", NODE, VERB, PAYLOAD, response.verb, response.verb_ext);


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


typedef struct {
    volatile HDA_Regs* regs;
    volatile HDA_StreamDescriptor* streamDescriptors;

    bool present;

    void* barAddress;

    u32* corbAddress;
    u32* rirbAddress;

    u32  corbWritePointerMask;
    u32  corbWritePointer;
    u32  rirbReadPointerMask;
    u32  rirbReadPointer;

    ScanInfo* scanInfo;
    PCI_ConfigSpace config;

    AudioDevice_impl* audioDevices[10];
    u32 audioDevices_len;
    u32 audioDevices_cap;

    int nextStreamNumber;
    int nextOutputStreamIndex;
    
} HDA_Controller;

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

// #########################
//     PUBLIC FUNCTIONS
// #########################

bool hda_scan(ScanInfo* scanInfo, PCI_ConfigSpace* config);



void hda_dump(HDA_Controller* dev);

const char* hda_widget_type_name(int type);

void print_widget_caps(u32 caps);

void print_pin_caps(u32 caps);

void print_config_default(u32 cfg);

void print_config_default(u32 cfg);

void print_supported_pcm(u32 supportedPcm, u32 supportedStreamFormats);

void print_amp_caps(u32 caps);

void hda_dump_stream(HDA_Controller* dev, int index);




void hda_reset(HDA_Controller* dev);

void hda_corb_dma_start(HDA_Controller* dev);

void hda_corb_dma_stop(HDA_Controller* dev);
void hda_rirb_dma_start(HDA_Controller* dev);
void hda_rirb_dma_stop(HDA_Controller* dev);

bool hda_send_verb(HDA_Controller* dev, u32 verb);
bool hda_receive_response(HDA_Controller* dev, HDA_RIRB_Entry* response);
bool hda_send_verb_sync(HDA_Controller* dev, u32 verb, HDA_RIRB_Entry* response);
u32 hda_make_verb(u8 codec, u8 node, u16 verb, u8 payload);
bool hda_get_param(HDA_Controller* dev, u8 codecAddress, u8 node, u16 param, HDA_RIRB_Entry* response);
int gain_index_from_db(u32 amp_cap, float db_level);
void hda_reset_stream(HDA_Controller* dev, int index);
void hda_stream_start(HDA_Controller* dev, int index);
void hda_stream_stop(HDA_Controller* dev, int index);
void hda_mute(HDA_Controller* dev, bool enabled);

void generate_sine(void* buffer, int size, u64* frameOffset);



bool hda_create_buffer(ELOS_AudioDevice device, ELOS_AudioFormat* format, u32 bufferSize, ELOS_AudioBuffer** buffer);
