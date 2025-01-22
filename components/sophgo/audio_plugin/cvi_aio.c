#include <alsa/asoundlib.h>

#include "cvi_aio.h"

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

static CVI_S32 cvi_ain_params(cvi_ain_t* ain)
{
    AIO_ATTR_S* pstAudinAttr = &ain->stAudinAttr;
    AI_TALKVQE_CONFIG_S* pstAiVqeTalkAttr = &ain->stAiVqeTalkAttr;

    ain->ch_cnt = CHANNELS;
    ain->sample_rate = SAMPLE_RATE;
    ain->bytes_per_sample = 2; // 16-bit
    ain->period = 100; // ms
    ain->vqe_on = CVI_TRUE;
    ain->AiDev = 0;
    ain->AiChn = 0;

    pstAudinAttr->enSamplerate = (AUDIO_SAMPLE_RATE_E)ain->sample_rate;
    pstAudinAttr->u32ChnCnt = 1;
    pstAudinAttr->enSoundmode = (ain->ch_cnt == 1) ? AUDIO_SOUND_MODE_MONO : AUDIO_SOUND_MODE_STEREO;
    pstAudinAttr->enBitwidth = AUDIO_BIT_WIDTH_16;
    pstAudinAttr->enWorkmode = AIO_MODE_I2S_MASTER;
    pstAudinAttr->u32EXFlag = 0;
    pstAudinAttr->u32FrmNum = 10; /* only use in bind mode */
    pstAudinAttr->u32PtNumPerFrm = (ain->sample_rate / 1000) * ain->period; /* sample_rate/fps */
    pstAudinAttr->u32ClkSel = 0;
    pstAudinAttr->enI2sType = AIO_I2STYPE_INNERCODEC;

    // vqe
    memset(pstAiVqeTalkAttr, 0, sizeof(AI_TALKVQE_CONFIG_S));
    pstAiVqeTalkAttr->s32WorkSampleRate = ain->sample_rate;
    _update_agc_anr_setting(pstAiVqeTalkAttr);
    _update_aec_setting(pstAiVqeTalkAttr);

    return 0;
}

CVI_S32 cvi_ain_init(cvi_ain_t* ain)
{
    cvi_ain_params(ain);

    CVI_S32 AiDev = ain->AiDev;
    CVI_S32 AiChn = ain->AiChn;
    CVI_S32 s32Ret = 0;

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

    if (ain->vqe_on) {
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
    }

    return CVI_SUCCESS;

ERROR1:
    // CVI_AI_DisableVqe(AiDev, AiChn);
    CVI_AI_DisableChn(AiDev, AiChn);
ERROR2:
    CVI_AI_Disable(AiDev);
ERROR3:
    return s32Ret;
}

CVI_S32 cvi_ain_get_frame(cvi_ain_t* ain, AUDIO_FRAME_S* pstFrame)
{
    return CVI_AI_GetFrame(ain->AiDev, ain->AiChn, pstFrame, NULL, -1);
}

CVI_S32 cvi_ain_deinit(cvi_ain_t* ain)
{
    if (ain->vqe_on) {
        CVI_AI_DisableVqe(ain->AiDev, ain->AiChn);
    }
    CVI_AI_DisableChn(ain->AiDev, ain->AiChn);
    CVI_AI_Disable(ain->AiDev);

    return 0;
}

static void _update_down_vqe_setting(cvi_aout_t* aout)
{
    AO_VQE_CONFIG_S* pstVqeConfig = &aout->stVqeConfig;
    int samplerate = aout->sample_rate;
    int channels = aout->ch_cnt;

    CVI_HPF_CONFIG_S stHpfParam;
    CVI_EQ_CONFIG_S stEqParam;
    CVI_DRC_COMPRESSOR_PARAM stDrcCompressor;
    CVI_DRC_LIMITER_PARAM stDrcLimiter;
    CVI_DRC_EXPANDER_PARAM stDrcExpander;

    pstVqeConfig->u32OpenMask = DNVQE_HPFILTER;
    pstVqeConfig->s32WorkSampleRate = samplerate;
    pstVqeConfig->s32channels = channels;
    // HPF
    stHpfParam.type = E_FILTER_HPF;
    stHpfParam.f0 = 70;
    stHpfParam.Q = 0.707;
    stHpfParam.gainDb = 0;
    pstVqeConfig->stHpfParam = stHpfParam;

    // EQ
    stEqParam.bandIdx = 5; /*bandIdx	0-5*/
    stEqParam.freq = 150; /*freq		100-20000*/
    stEqParam.QValue = 0.707; /*QValue	0.01-10*/
    stEqParam.gainDb = 6; /*gainDb	-30-30*/
    pstVqeConfig->stEqParam = stEqParam;

    // DRC-Limiter
    stDrcLimiter.attackTimeMs = 10;
    stDrcLimiter.releaseTimeMs = 100;
    stDrcLimiter.thresholdDb = -2;
    stDrcLimiter.postGain = 4.0f;
    pstVqeConfig->stDrcLimiter = stDrcLimiter;

    // DRC-Compressor
    stDrcCompressor.attackTimeMs = 5;
    stDrcCompressor.releaseTimeMs = 200;
    stDrcCompressor.thresholdDb = -2;
    stDrcCompressor.ratio = 10;
    pstVqeConfig->stDrcCompressor = stDrcCompressor;

    // DRC-pExpander
    stDrcExpander.attackTimeMs = 10;
    stDrcExpander.releaseTimeMs = 200;
    stDrcExpander.thresholdDb = -20;
    stDrcExpander.minDb = -60;
    stDrcExpander.ratio = 5;
    stDrcExpander.holdTimeMs = 20;
    pstVqeConfig->stDrcExpander = stDrcExpander;
}

static CVI_S32 cvi_aout_params(cvi_aout_t* aout)
{
    AIO_ATTR_S* pstAudoutAttr = &aout->stAudoutAttr;

    aout->ch_cnt = CHANNELS;
    aout->sample_rate = SAMPLE_RATE;
    aout->bytes_per_sample = 2; // 16-bit
    aout->period = 10; // PERIOD_SIZE;
    aout->vqe_on = CVI_TRUE;

    aout->AoDev = 0;
    aout->AoChn = 0;

    pstAudoutAttr->enSamplerate = (AUDIO_SAMPLE_RATE_E)aout->sample_rate;
    pstAudoutAttr->u32ChnCnt = 1;
    pstAudoutAttr->enSoundmode = (aout->ch_cnt == 1) ? AUDIO_SOUND_MODE_MONO : AUDIO_SOUND_MODE_STEREO;
    pstAudoutAttr->enBitwidth = AUDIO_BIT_WIDTH_16;
    pstAudoutAttr->enWorkmode = AIO_MODE_I2S_MASTER;
    pstAudoutAttr->u32EXFlag = 0;
    pstAudoutAttr->u32FrmNum = 10; /* only use in bind mode */
    pstAudoutAttr->u32PtNumPerFrm = (aout->sample_rate / 1000) * aout->period; /* sample_rate/fps */
    pstAudoutAttr->u32ClkSel = 0;
    pstAudoutAttr->enI2sType = AIO_I2STYPE_INNERCODEC;

    // vqe
    _update_down_vqe_setting(aout);

    return 0;
}

CVI_S32 cvi_aout_init(cvi_aout_t* aout)
{
    cvi_aout_params(aout);

    CVI_S32 AoDev = aout->AoDev;
    CVI_S32 AoChn = aout->AoChn;
    CVI_S32 s32Ret = 0;

    s32Ret = CVI_AO_SetPubAttr(AoDev, &aout->stAudoutAttr);
    if (s32Ret != CVI_SUCCESS) {
        printf("[cvi_error],[%s],[line:%d],\n", __func__, __LINE__);
        goto ERROR3;
    }
    s32Ret = CVI_AO_Enable(AoDev);
    if (s32Ret != CVI_SUCCESS) {
        printf("[cvi_error],[%s],[line:%d],\n", __func__, __LINE__);
        goto ERROR3;
    }

    s32Ret = CVI_AO_EnableChn(AoDev, AoChn);
    if (s32Ret != CVI_SUCCESS) {
        printf("[cvi_error],[%s],[line:%d],\n", __func__, __LINE__);
        goto ERROR2;
    }

    if (aout->vqe_on) {
        s32Ret = CVI_AO_SetVqeAttr(AoDev, AoChn, &aout->stVqeConfig);
        if (s32Ret != CVI_SUCCESS) {
            printf("[cvi_error],[%s],[line:%d],\n", __func__, __LINE__);
            goto ERROR1;
        }
        s32Ret = CVI_AO_EnableVqe(AoDev, AoChn);
        if (s32Ret != CVI_SUCCESS) {
            printf("[cvi_error],[%s],[line:%d],\n", __func__, __LINE__);
            goto ERROR1;
        }
    }

    return CVI_SUCCESS;

ERROR1:
    // CVI_AO_DisableVqe(AoDev, AoChn);
    CVI_AO_DisableChn(AoDev, AoChn);
ERROR2:
    CVI_AO_Disable(AoDev);
ERROR3:
    return s32Ret;
}

CVI_S32 cvi_aout_put_frame(cvi_aout_t* aout, const AUDIO_FRAME_S* pstFrame)
{
    return CVI_AO_SendFrame(aout->AoDev, aout->AoChn, pstFrame, 1000);
}

CVI_S32 cvi_aout_deinit(cvi_aout_t* aout)
{
    if (aout->vqe_on) {
        CVI_AO_DisableVqe(aout->AoDev, aout->AoChn);
    }
    CVI_AO_DisableChn(aout->AoDev, aout->AoChn);
    CVI_AO_Disable(aout->AoDev);

    return 0;
}
