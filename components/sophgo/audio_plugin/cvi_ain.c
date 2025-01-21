#include <alsa/asoundlib.h>

#include "cvi_ain.h"

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
    // printf("pstAiVqeTalkAttr:u32OpenMask[0x%x]\n", pstAiVqeTalkAttr->u32OpenMask);
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
    // printf("pstAiVqeTalkAttr:u32OpenMask[0x%x]\n", pstAiVqeTalkAttr->u32OpenMask);
    return CVI_FALSE;
}

static CVI_S32 cvi_audio_params(cvi_ain_t* ain)
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
    pstAudinAttr->u32PtNumPerFrm = SAMPLE_RATE / 10; /* sample_rate/fps */
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
    cvi_audio_params(ain);

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
