#include <alsa/asoundlib.h>
#include <cstring>
#include <fstream>
#include <iostream>

#include "audio.h"

#define SAMPLE_RATE 16000
#define CHANNELS 1
#define FORMAT SND_PCM_FORMAT_S16_LE
#define DURATION 5

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
void write_wav_header(std::ofstream& file, unsigned int data_size)
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

typedef struct cvi_ain {
    int AiDev;
    int AiChn;
    AIO_ATTR_S stAudinAttr;
    AI_TALKVQE_CONFIG_S stAiVqeTalkAttr;
} cvi_ain_t;

static CVI_BOOL _update_agc_anr_setting(AI_TALKVQE_CONFIG_S* pstAiVqeTalkAttr)
{
    if (pstAiVqeTalkAttr == NULL)
        return CVI_FALSE;

    pstAiVqeTalkAttr->u32OpenMask |= (NR_ENABLE | AGC_ENABLE | DCREMOVER_ENABLE);

    AUDIO_AGC_CONFIG_S st_AGC_Setting;
    AUDIO_ANR_CONFIG_S st_ANR_Setting;

    st_AGC_Setting.para_agc_max_gain = 0;
    st_AGC_Setting.para_agc_target_high = 2;
    st_AGC_Setting.para_agc_target_low = 72;
    st_AGC_Setting.para_agc_vad_ena = CVI_TRUE;
    st_ANR_Setting.para_nr_snr_coeff = 15;
    st_ANR_Setting.para_nr_init_sile_time = 0;

    pstAiVqeTalkAttr->stAgcCfg = st_AGC_Setting;
    pstAiVqeTalkAttr->stAnrCfg = st_ANR_Setting;

    pstAiVqeTalkAttr->para_notch_freq = 0;
    printf("pstAiVqeTalkAttr:u32OpenMask[0x%x]\n", pstAiVqeTalkAttr->u32OpenMask);
    return CVI_TRUE;
}

static CVI_BOOL _update_aec_setting(AI_TALKVQE_CONFIG_S* pstAiVqeTalkAttr)
{
    if (pstAiVqeTalkAttr == NULL)
        return CVI_FALSE;

    AI_AEC_CONFIG_S default_AEC_Setting;

    memset(&default_AEC_Setting, 0, sizeof(AI_AEC_CONFIG_S));
    default_AEC_Setting.para_aec_filter_len = 13;
    default_AEC_Setting.para_aes_std_thrd = 37;
    default_AEC_Setting.para_aes_supp_coeff = 60;
    pstAiVqeTalkAttr->stAecCfg = default_AEC_Setting;
    pstAiVqeTalkAttr->u32OpenMask = LP_AEC_ENABLE | NLP_AES_ENABLE | NR_ENABLE | AGC_ENABLE;
    printf("pstAiVqeTalkAttr:u32OpenMask[0x%x]\n", pstAiVqeTalkAttr->u32OpenMask);
    return CVI_FALSE;
}

CVI_S32 cvi_audio_params(cvi_ain_t* ain)
{
    AIO_ATTR_S* pstAudinAttr = &ain->stAudinAttr;
    AI_TALKVQE_CONFIG_S* pstAiVqeTalkAttr = &ain->stAiVqeTalkAttr;

    ain->AiDev = 0;
    ain->AiChn = 0;

    // AIO_ATTR_S AudinAttr;
    pstAudinAttr->enSamplerate = (AUDIO_SAMPLE_RATE_E)SAMPLE_RATE;
    pstAudinAttr->u32ChnCnt = 3;
    pstAudinAttr->enSoundmode = AUDIO_SOUND_MODE_MONO;
    pstAudinAttr->enBitwidth = AUDIO_BIT_WIDTH_16;
    pstAudinAttr->enWorkmode = AIO_MODE_I2S_MASTER;
    pstAudinAttr->u32EXFlag = 0;
    pstAudinAttr->u32FrmNum = 10; /* only use in bind mode */
    pstAudinAttr->u32PtNumPerFrm = SAMPLE_RATE; /* sample_rate/fps */
    pstAudinAttr->u32ClkSel = 0;
    pstAudinAttr->enI2sType = AIO_I2STYPE_INNERCODEC;

    // vqe
    memset(pstAiVqeTalkAttr, 0, sizeof(AI_TALKVQE_CONFIG_S));

    pstAiVqeTalkAttr->s32WorkSampleRate = SAMPLE_RATE;
    _update_agc_anr_setting(pstAiVqeTalkAttr);
    _update_aec_setting(pstAiVqeTalkAttr);

    return 0;
}

CVI_S32 cvi_audio_init(cvi_ain_t* ain)
{
    int AiDev = ain->AiDev;
    int AiChn = ain->AiChn;
    int s32Ret = 0;

    s32Ret = CVI_AUDIO_INIT();
    if (s32Ret != CVI_SUCCESS) {
        printf("[error],[%s],[line:%d],\n", __func__, __LINE__);
        goto ERROR3;
    }

    s32Ret = CVI_AI_SetPubAttr(AiDev, &ain->stAudinAttr);
    if (s32Ret != CVI_SUCCESS) {
        printf("[error],[%s],[line:%d],\n", __func__, __LINE__);
        goto ERROR3;
    }
    s32Ret = CVI_AI_Enable(AiDev);
    if (s32Ret != CVI_SUCCESS) {
        printf("[error],[%s],[line:%d],\n", __func__, __LINE__);
        goto ERROR3;
    }

    s32Ret = CVI_AI_EnableChn(AiDev, AiChn);
    if (s32Ret != CVI_SUCCESS) {
        printf("[error],[%s],[line:%d],\n", __func__, __LINE__);
        goto ERROR2;
    }

    s32Ret = CVI_AI_SetTalkVqeAttr(AiDev, AiChn, 0, 0, &ain->stAiVqeTalkAttr);
    if (s32Ret != CVI_SUCCESS) {
        printf("[error],[%s],[line:%d],\n", __func__, __LINE__);
        goto ERROR1;
    }

    s32Ret = CVI_AI_EnableVqe(AiDev, AiChn);
    if (s32Ret != CVI_SUCCESS) {
        printf("[error],[%s],[line:%d],\n", __func__, __LINE__);
        goto ERROR1;
    }

    return 0;

ERROR1:
    CVI_AI_DisableVqe(AiDev, AiChn);
    CVI_AI_DisableChn(AiDev, AiChn);
ERROR2:
    CVI_AI_Disable(AiDev);
ERROR3:
    CVI_AUDIO_DEINIT();

    return s32Ret;
}

CVI_S32 cvi_audio_get_frame(cvi_ain_t* ain, AUDIO_FRAME_S* pstFrame, AEC_FRAME_S* pstAecFrm)
{
    return CVI_AI_GetFrame(ain->AiDev, ain->AiChn, pstFrame, pstAecFrm, -1);
}

CVI_S32 cvi_audio_deinit(cvi_ain_t* ain)
{
    CVI_AI_DisableVqe(ain->AiDev, ain->AiChn);
    CVI_AI_DisableChn(ain->AiDev, ain->AiChn);
    CVI_AI_Disable(ain->AiDev);
    CVI_AUDIO_DEINIT();

    return 0;
}

int main(int argc, char* argv[])
{
    std::ofstream out_file("cvi_ain.wav", std::ios::binary);

    write_wav_header(out_file, DURATION * SAMPLE_RATE * CHANNELS * 2);

    cvi_ain_t ain;
    AUDIO_FRAME_S stFrame;
    AEC_FRAME_S stAecFrm;

    cvi_audio_params(&ain);
    cvi_audio_init(&ain);

    for (int i = 0; i < DURATION; i++) {
        if (cvi_audio_get_frame(&ain, &stFrame, &stAecFrm)) {
            printf("cvi_audio_get_frame error\n");
            return -1;
        }
        printf("stFrame.u32Len:%d\n", stFrame.u32Len);
        out_file.write((const char*)stFrame.u64VirAddr[0], stFrame.u32Len * CHANNELS * 2);
    }

    cvi_audio_deinit(&ain);
    out_file.close();

    return 0;
}
