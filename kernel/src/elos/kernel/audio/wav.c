
#include "wav.h"

#include "elos/common/types.h"
#include "elos/common/string.h"

#include "elos/kernel_console.h"
#include "elos/physical_memory.h"
#include "elos/vfs.h"



#define printf(...) KCON_printf(__VA_ARGS__)
#define debug(...) printf(__VA_ARGS__)


static bool equal_bytes(const void* a, const void* b, size_t len) {
    return memcmp(a, b, len) == 0;
}

#define Free(S) PMEM_free(S)
#define Allocate(S) PMEM_alloc(S)


bool ReadWholeFile(const char* path, uint8_t** data, uint32_t* data_len) {
    VFS_Handle file = VFS_open(path, VFS_FLAG_READ_ONLY);
    if (file == VFS_NULL_HANDLE) {
        return false;
    
    }
    VFS_HandleInfo info;
    VFS_info(file, &info);

    void* buffer = PMEM_alloc_phys(info.fileSize, PMEM_FLAG_IDENTITY_MAPPED | PMEM_FLAG_NOT_CACHED);

    VFS_read(file, 0, info.fileSize, buffer);
    VFS_close(file);

    *data = (uint8_t*)buffer;
    *data_len = info.fileSize;

    return true;
}


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
) {
    uint8_t* data;
    uint32_t data_len;

    bool ok = ReadWholeFile(path, &data, &data_len);

    if (!ok) {
        if (print)
            printf("ReadWAVFile: Could not read %s\n", path);

        return WAV_FILE_NOT_FOUND;
    }


    WAVFile* wav = NULL;

    WAVError err = ParseWAVHeader(
        data,
        data_len,
        &wav,
        print,
        path
    );

    if (err != WAV_SUCCESS) {
        Free(data);
        return WAV_CORRUPT;
    }


    err = ParseWAVChunks(
        wav,
        data + WAVFILE_HEADER_SIZE,
        data_len - WAVFILE_HEADER_SIZE,
        false,
        print,
        path
    );


    Free(data);


    if (err != WAV_SUCCESS) {
        DestroyWAVFile(wav);
        return WAV_CORRUPT;
    }


    *out_wav = wav;
    return WAV_SUCCESS;
}



WAVError ParseWAVHeader(
    uint8_t* data,
    uint32_t data_len,
    WAVFile** out_wav,
    bool print,
    const char* path
) {
    uint32_t head = 0;


    if (data_len < WAVFILE_HEADER_SIZE) {
        if (print)
            printf(
                "ParseWAVHeader: file too small (%u)\n",
                data_len
            );

        return WAV_CORRUPT;
    }


    if (!equal_bytes(data + head, "RIFF", 4)) {
        if (print)
            printf("ParseWAVHeader: RIFF missing\n");

        return WAV_CORRUPT;
    }

    head += 4;


    int32_t almost_file_size =
        *(int32_t*)(data + head);

    head += 4;


    if (!equal_bytes(data + head, "WAVE", 4)) {
        if (print)
            printf("ParseWAVHeader: WAVE missing\n");

        return WAV_CORRUPT;
    }

    head += 4;


    if (!equal_bytes(data + head, "fmt ", 4)) {
        if (print)
            printf("ParseWAVHeader: fmt missing\n");

        return WAV_CORRUPT;
    }

    head += 4;


    int32_t format_size =
        *(int32_t*)(data + head);

    head += 4;


    if (format_size != 16) {
        if (print)
            printf(
                "ParseWAVHeader: unsupported fmt size %d\n",
                format_size
            );

        return WAV_CORRUPT;
    }


    WAVFile* wav = Allocate(sizeof(WAVFile));

    memset(wav, 0, sizeof(*wav));


    wav->m_size_of_chunks =
        almost_file_size - WAVFILE_HEADER_SIZE + 8;


    uint16_t audio_format =
        *(uint16_t*)(data + head);

    uint16_t channels =
        *(uint16_t*)(data + head + 2);

    uint32_t sample_rate =
        *(uint32_t*)(data + head + 4);

    uint32_t byte_rate =
        *(uint32_t*)(data + head + 8);

    uint16_t block_align =
        *(uint16_t*)(data + head + 12);

    uint16_t bits_per_sample =
        *(uint16_t*)(data + head + 14);


    head += 16;


    if (byte_rate != sample_rate * (bits_per_sample / 8) * channels)
        return WAV_CORRUPT;

    if (block_align != (bits_per_sample / 8) * channels)
        return WAV_CORRUPT;


    if (audio_format != WAVFILE_AUDIO_FORMAT_PCM_INTEGER &&
        audio_format != WAVFILE_AUDIO_FORMAT_IEEE_FLOAT)
        return WAV_CORRUPT;


    AudioFormat_init(
        &wav->format,
        sample_rate,
        channels,
        bits_per_sample,
        audio_format == WAVFILE_AUDIO_FORMAT_IEEE_FLOAT
    );


    *out_wav = wav;

    return WAV_SUCCESS;
}



WAVError ParseWAVChunks(
    WAVFile* wav,
    uint8_t* data,
    uint32_t data_len,
    bool use_passed_data_memory,
    bool print,
    const char* path
) {
    uint32_t head = 0;


    while (head < data_len) {

        uint8_t* chunk_id = data + head;
        head += 4;


        uint32_t chunk_len =
            *(uint32_t*)(data + head);

        head += 4;



        if (equal_bytes(chunk_id, "data", 4)) {

            if (data_len - head < chunk_len) {

                if (print)
                    printf(
                        "ParseWAVChunks: invalid data size\n"
                    );

                return WAV_CORRUPT;
            }


            wav->data_len = chunk_len;


            uint32_t frame_size =
                AudioFormat_get_frame_size(&wav->format);


            if (chunk_len % frame_size != 0) {

                if (print)
                    printf(
                        "ParseWAVChunks: bad block alignment\n"
                    );

                return WAV_CORRUPT;
            }


            if (use_passed_data_memory) {

                wav->data = data + head;

            } else {

                wav->data = Allocate(chunk_len);

                if (!wav->data)
                    return WAV_CORRUPT;


                memcpy(
                    wav->data,
                    data + head,
                    chunk_len
                );

                wav->owner_of_data_allocation = true;
            }


            return WAV_SUCCESS;
        }


        // Skip unknown chunk
        head += chunk_len;
    }


    if (print)
        printf(
            "ParseWAVChunks: data chunk missing\n"
        );


    return WAV_CORRUPT;
}


void DestroyWAVFile(WAVFile* wav) {
    // if wav.data && wav.owner_of_data_allocation
    //     Free(wav.data, wav.data_len)
    // Free(wav, sizeof(WAVFile))
}


bool AudioFormat_init(
    AudioFormat* format,
    int32_t sample_rate,
    int32_t channels,
    int32_t bits_per_sample,
    bool is_float
) {
    if (channels != 1 && channels != 2)
        return false;

    format->sample_rate = sample_rate;
    format->stereo = (channels == 2);


    if (is_float) {

        if (bits_per_sample != 32)
            return false;

        format->sample_format = AUDIO_32BIT_FLOAT;

    } else {

        switch (bits_per_sample) {

            case 8:
                format->sample_format = AUDIO_8BIT_PCM;
                break;

            case 16:
                format->sample_format = AUDIO_16BIT_PCM;
                break;

            case 32:
                format->sample_format = AUDIO_32BIT_PCM;
                break;

            default:
                return false;
        }
    }

    return true;
}



int32_t AudioFormat_frames_from_duration(
    const AudioFormat* format,
    float time
) {
    return (int32_t)(format->sample_rate * time);
}



int32_t AudioFormat_get_channels(
    const AudioFormat* format
) {
    return format->stereo ? 2 : 1;
}



int32_t AudioFormat_get_bits_per_sample(
    const AudioFormat* format
) {
    switch (format->sample_format) {

        case AUDIO_8BIT_PCM:
            return 8;

        case AUDIO_16BIT_PCM:
            return 16;

        case AUDIO_32BIT_PCM:
        case AUDIO_32BIT_FLOAT:
            return 32;
    }

    return -1;
}



int32_t AudioFormat_get_frame_size(
    const AudioFormat* format
) {
    return
        (AudioFormat_get_bits_per_sample(format) / 8) *
        AudioFormat_get_channels(format);
}



int32_t AudioFormat_byte_size_from_duration(
    const AudioFormat* format,
    float time
) {
    int32_t frames =
        (int32_t)(format->sample_rate * time);

    return AudioFormat_get_frame_size(format) * frames;
}


