#include <alsa/asoundlib.h>
#include <cstring>
#include <fstream>
#include <iostream>

#include "audio.h"
#include "cvi_aio.h"

// #define CHANNELS 1
// #define SAMPLE_RATE 16000
// #define PERIOD_SIZE 100 // 100ms

// WAV 文件头结构
struct WAVHeader {
    char riff[4]; // "RIFF"
    unsigned int size; // 4 + (8 + size of subchunks)
    char wave[4]; // "WAVE"
    char fmt[4]; // "fmt "
    unsigned int fmt_size; // 16 for PCM
    unsigned short audio_format; // 1 for PCM
    unsigned short num_channels; // 1 or 2
    unsigned int sample_rate; // 16000
    unsigned int byte_rate; // sample_rate * num_channels * bytes_per_sample
    unsigned short block_align; // num_channels * bytes_per_sample
    unsigned short bits_per_sample; // 16
    char data[4]; // "data"
    unsigned int data_size; // num_samples * num_channels * bytes_per_sample
};

// WAV 文件写入
static void write_wav_header(std::ofstream& file, unsigned int data_size)
{
    WAVHeader header;
    std::memset(&header, 0, sizeof(header));

    std::strcpy(header.riff, "RIFF");
    header.size = 36 + data_size;
    std::strcpy(header.wave, "WAVE");
    std::strcpy(header.fmt, "fmt ");
    header.fmt_size = 16;
    header.audio_format = 1; // PCM
    header.num_channels = CHANNELS;
    header.sample_rate = SAMPLE_RATE;
    header.byte_rate = SAMPLE_RATE * CHANNELS * 2; // 16-bit (2 bytes)
    header.block_align = CHANNELS * 2;
    header.bits_per_sample = 16;
    std::strcpy(header.data, "data");
    header.data_size = data_size;

    printf("0x%x\n", data_size);
    file.write(reinterpret_cast<char*>(&header), sizeof(header));
}

static int ain(const char* filename, uint32_t time_ms)
{
    cvi_ain_t ain;
    CVI_S32 s32Ret = cvi_ain_init(&ain);
    if (CVI_SUCCESS != s32Ret) {
        printf("[error],[%s],[line:%d],\n", __func__, __LINE__);
        return s32Ret;
    }

    std::ofstream out_file(filename, std::ios::binary);
    uint32_t frames = time_ms / ain.period;
    uint32_t sample_size = ain.ch_cnt * ain.bytes_per_sample;
    uint32_t data_size = frames * ain.stAudinAttr.u32PtNumPerFrm * sample_size;

    write_wav_header(out_file, data_size);

    for (int i = 0; i < frames; i++) {
        AUDIO_FRAME_S stFrame;
        if (cvi_ain_get_frame(&ain, &stFrame)) {
            printf("cvi_audio_get_frame error\n");
            break;
        }
        printf("[%d]stFrame.u32Len:%d\n", i, stFrame.u32Len);
        out_file.write((const char*)stFrame.u64VirAddr[0], stFrame.u32Len * sample_size);
    }

    out_file.close();
    cvi_ain_deinit(&ain);

    return s32Ret;
}

static int aout(const char* filename)
{
    cvi_aout_t aout;
    CVI_S32 s32Ret = cvi_aout_init(&aout);
    if (CVI_SUCCESS != s32Ret) {
        printf("[error],[%s],[line:%d],\n", __func__, __LINE__);
        return s32Ret;
    }

    printf("u32PtNumPerFrm=%d\n", aout.stAudoutAttr.u32PtNumPerFrm);
    uint32_t sample_bytes = aout.ch_cnt * aout.bytes_per_sample;
    CVI_S32 frame_bytes = aout.stAudoutAttr.u32PtNumPerFrm * sample_bytes;
    printf("sample_bytes=%d\n", sample_bytes);
    printf("frame_bytes=%d\n", frame_bytes);

    char* pBuffer = NULL;
    WAVHeader header;
    uint32_t wav_sample_bytes;
    uint32_t wav_samples;
    std::ifstream in_file(filename, std::ios::binary);
    in_file.read(reinterpret_cast<char*>(&header), sizeof(WAVHeader));
    if (*(int*)header.wave != 0x45564157) {
        printf("Not a wav file\n");
        goto EXIT_AOUT;
    }
    wav_sample_bytes = (header.bits_per_sample / 8) * header.num_channels;
    wav_samples = header.data_size / wav_sample_bytes;
    printf("wav header.data_size=%d\n", header.data_size);
    printf("wav_sample_bytes=%d\n", wav_sample_bytes);
    printf("wav_samples=%d\n", wav_samples);

    pBuffer = (char*)malloc(frame_bytes);
    if (pBuffer == NULL) {
        printf("malloc error\n");
        goto EXIT_AOUT;
    }
    for (uint32_t i = 0; i < wav_samples / aout.stAudoutAttr.u32PtNumPerFrm; i++) {
        memset(pBuffer, 0, frame_bytes);
        in_file.read(pBuffer, frame_bytes);
        uint32_t readbytes = static_cast<uint32_t>(in_file.gcount());
        if (readbytes <= 0)
            break;

        printf("[%d]readbytes=%d, frame_bytes=%d\n", i, readbytes, frame_bytes);

        AUDIO_FRAME_S stFrame;
        stFrame.u64VirAddr[0] = (CVI_U8*)pBuffer;
        stFrame.u32Len = aout.stAudoutAttr.u32PtNumPerFrm;
        stFrame.u64TimeStamp = 0;
        stFrame.enSoundmode = aout.stAudoutAttr.enSoundmode;
        stFrame.enBitwidth = AUDIO_BIT_WIDTH_16;
        s32Ret = cvi_aout_put_frame(&aout, &stFrame);
        if (s32Ret != CVI_SUCCESS) {
            printf("[cvi_info] CVI_AO_SendFrame failed with %#x!\n", s32Ret);
        }
    }
    free(pBuffer);

EXIT_AOUT:
    in_file.close();
    cvi_aout_deinit(&aout);

    return s32Ret;
}

int main(int argc, char* argv[])
{
    CVI_S32 s32Ret = CVI_AUDIO_INIT();
    if (s32Ret != CVI_SUCCESS) {
        printf("[error],[%s],[line:%d],\n", __func__, __LINE__);
        return s32Ret;
    }

    if (argc == 1) {
        printf("Record: cvi_ain.wav\n");
        ain("cvi_ain.wav", 5000 /*ms*/);
    } else {
        printf("Play: %s\n", argv[1]);
        aout(argv[1]);
    }

EXIT:
    CVI_AUDIO_DEINIT();
    return 0;
}
