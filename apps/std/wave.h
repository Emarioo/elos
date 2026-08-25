/*

    Useful WAV parser copied from BTB project.

    @TODO Move out of std?

*/


#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef enum {
    AUDIO_8BIT_PCM,
    AUDIO_16BIT_PCM,
    AUDIO_32BIT_PCM,
    AUDIO_32BIT_FLOAT,
} AudioSampleFormat;


typedef struct {
    AudioSampleFormat sample_format;
    int32_t sample_rate;
    bool stereo;
} AudioFormat;


#define WAVFILE_HEADER_SIZE 36
#define WAVFILE_AUDIO_FORMAT_PCM_INTEGER 1
#define WAVFILE_AUDIO_FORMAT_IEEE_FLOAT 3

typedef enum {
    WAV_SUCCESS,
    WAV_CORRUPT,
    WAV_FILE_NOT_FOUND,
} WAVError;


typedef struct {
    AudioFormat format;

    bool owner_of_data_allocation;
    uint8_t* data;
    int32_t data_len;

    int32_t m_size_of_chunks;
} WAVFile;


// Forward declarations
WAVError ParseWAVChunks(
    WAVFile* wav,
    uint8_t* data,
    uint32_t data_len,
    bool use_passed_data_memory,
    bool print,
    const char* path
);


WAVError ParseWAVHeader(
    uint8_t* data,
    uint32_t data_len,
    WAVFile** out_wav,
    bool print,
    const char* path
);


WAVError ReadWAVFile(
    const char* path,
    WAVFile** out_wav,
    bool print
);

bool AudioFormat_init(
    AudioFormat* format,
    int32_t sample_rate,
    int32_t channels,
    int32_t bits_per_sample,
    bool is_float
);


int32_t AudioFormat_frames_from_duration(
    const AudioFormat* format,
    float time
);


int32_t AudioFormat_get_channels(
    const AudioFormat* format
);

int32_t AudioFormat_get_bits_per_sample(
    const AudioFormat* format
);


int32_t AudioFormat_get_frame_size(
    const AudioFormat* format
);


int32_t AudioFormat_byte_size_from_duration(
    const AudioFormat* format,
    float time
);


void DestroyWAVFile(WAVFile* wav);
