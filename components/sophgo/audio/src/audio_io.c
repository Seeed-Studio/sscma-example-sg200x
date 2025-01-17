#include "audio_io.h"

#define SMP_AUD_UNUSED_REF(X) ((X) = (X))
#define AUDIO_ADPCM_TYPE ADPCM_TYPE_DVI4 /* ADPCM_TYPE_IMA, ADPCM_TYPE_DVI4*/
#define G726_BPS MEDIA_G726_32K /* MEDIA_G726_16K, MEDIA_G726_24K ... */

static AAC_TYPE_E gs_enAacType = AAC_TYPE_AACLC;
static AAC_BPS_E gs_enAacBps = AAC_BPS_32K;
static AAC_TRANS_TYPE_E gs_enAacTransType = AAC_TRANS_TYPE_ADTS;

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

static CVI_S32 _update_aenc_params(AENC_CHN_ATTR_S* pAencAttrs, AIO_ATTR_S* pAioAttrs,
    PAYLOAD_TYPE_E enType)
{
    CVI_S32 s32Ret = CVI_SUCCESS;

    if (!pAencAttrs || !pAioAttrs) {
        printf("[fatal error][%p, %p] ptr is NULL,fuc:%s,line:%d\n",
            pAencAttrs, pAioAttrs, __func__, __LINE__);
        return -1;
    }

    memset(pAencAttrs, 0, sizeof(AENC_CHN_ATTR_S));
    pAencAttrs->enType = enType;
    pAencAttrs->u32BufSize = 30;
    pAencAttrs->u32PtNumPerFrm = pAioAttrs->u32PtNumPerFrm;

    if (pAencAttrs->enType == PT_ADPCMA) {
        AENC_ATTR_ADPCM_S* pstAdpcmAenc = (AENC_ATTR_ADPCM_S*)malloc(sizeof(
            AENC_ATTR_ADPCM_S));

        pstAdpcmAenc->enADPCMType = AUDIO_ADPCM_TYPE;
        pAencAttrs->pValue = (CVI_VOID*)pstAdpcmAenc;
    } else if (pAencAttrs->enType == PT_G711A || pAencAttrs->enType == PT_G711U) {
        AENC_ATTR_G711_S* pstAencG711 = (AENC_ATTR_G711_S*)malloc(sizeof(
            AENC_ATTR_G711_S));
        pAencAttrs->pValue = (CVI_VOID*)pstAencG711;
    } else if (pAencAttrs->enType == PT_G726) {
        AENC_ATTR_G726_S* pstAencG726 = (AENC_ATTR_G726_S*)malloc(sizeof(
            AENC_ATTR_G726_S));
        pstAencG726->enG726bps = G726_BPS;
        pAencAttrs->pValue = (CVI_VOID*)pstAencG726;
    } else if (pAencAttrs->enType == PT_LPCM) {
        AENC_ATTR_LPCM_S* pstAencLpcm = (AENC_ATTR_LPCM_S*)malloc(sizeof(
            AENC_ATTR_LPCM_S));
        pAencAttrs->pValue = (CVI_VOID*)pstAencLpcm;
    } else if (pAencAttrs->enType == PT_AAC) {
        printf("Need update detail external AAC function params\n");
        // need update AAC if supported
    } else {
        printf("[Error]Not support codec type[%d]\n", enType);
        s32Ret = CVI_FAILURE;
    }

    return s32Ret;
}

static CVI_S32 _update_Aenc_setting(AIO_ATTR_S* pstAioAttr,
    AENC_CHN_ATTR_S* pstAencAttr,
    PAYLOAD_TYPE_E enType,
    int Sample_rate, bool bVqe)
{
    CVI_S32 s32Ret;
    SMP_AUD_UNUSED_REF(bVqe);

    s32Ret = _update_aenc_params(pstAencAttr, pstAioAttr, enType);
    if (s32Ret != CVI_SUCCESS) {
        printf("[Error][%s]failure in params\n", __func__);
        return CVI_FAILURE;
    }

    if (enType == PT_AAC) {
        AENC_ATTR_AAC_S* pstAencAac = (AENC_ATTR_AAC_S*)malloc(sizeof(
            AENC_ATTR_AAC_S));
        pstAencAac->enAACType = gs_enAacType;
        pstAencAac->enBitRate = gs_enAacBps;
        pstAencAac->enBitWidth = AUDIO_BIT_WIDTH_16;

        pstAencAac->enSmpRate = Sample_rate;
        printf("[cvi_info] AAC enc[%s][%d]smp-rate[%d]\n",
            __func__,
            __LINE__,
            pstAencAac->enSmpRate);

        pstAencAac->enSoundMode = pstAioAttr->enSoundmode;
        pstAencAac->enTransType = gs_enAacTransType;
        pstAencAac->s16BandWidth = 0;
        pstAencAttr->pValue = pstAencAac;
        s32Ret = CVI_MPI_AENC_AacInit();
        printf("[cvi_info] CVI_MPI_AENC_AacInit s32Ret:%d\n", s32Ret);
    }

    return CVI_SUCCESS;
}

static int _destroy_Aenc_setting(AENC_CHN_ATTR_S* pstAencAttr)
{
    if (!pstAencAttr || !pstAencAttr->pValue) {
        printf("[fatal error][%p, %p] ptr is NULL,fuc:%s,line:%d\n",
            pstAencAttr, pstAencAttr->pValue, __func__, __LINE__);
        return -1;
    }
    if (pstAencAttr->enType == PT_AAC)
        CVI_MPI_AENC_AacDeInit();
    if (pstAencAttr->pValue) {
        free(pstAencAttr->pValue);
        pstAencAttr->pValue = NULL;
    }
    memset(pstAencAttr, 0, sizeof(AENC_CHN_ATTR_S));

    return 0;
}

void* Thread_AudioIn_Proc(void* arg)
{
    thread_aio_context_t* pstThreadAioCtx = (thread_aio_context_t*)arg;

    CVI_S32 s32Ret = 0;
    bool bVqe = true;
    bool bReSmp = false;
    int ChCnt = 2;
    int AiDev = 0;
    int AiChn = 0;
    int AeChn = 0;

    AUDIO_SAMPLE_RATE_E Chnsample_rate = (AUDIO_SAMPLE_RATE_E)pstThreadAioCtx->enRate;
    PAYLOAD_TYPE_E enType = pstThreadAioCtx->enType;

    AIO_ATTR_S AudinAttr;
    AudinAttr.enSamplerate = Chnsample_rate;
    AudinAttr.u32ChnCnt = 1;
    AudinAttr.enSoundmode = (ChCnt == 2 ? AUDIO_SOUND_MODE_STEREO : AUDIO_SOUND_MODE_MONO);
    AudinAttr.enBitwidth = AUDIO_BIT_WIDTH_16;
    AudinAttr.enWorkmode = AIO_MODE_I2S_MASTER;
    AudinAttr.u32EXFlag = 0;
    AudinAttr.u32FrmNum = 10; /* only use in bind mode */
    AudinAttr.u32PtNumPerFrm = (PT_AAC == enType) ? 1024 : 480; /* sample_rate/fps */
    AudinAttr.u32ClkSel = 0;
    AudinAttr.enI2sType = AIO_I2STYPE_INNERCODEC;

    s32Ret = CVI_AUDIO_INIT();
    if (s32Ret != CVI_SUCCESS) {
        printf("[error],[%s],[line:%d],\n", __func__, __LINE__);
        goto ERROR3;
    }

    s32Ret = CVI_AI_SetPubAttr(AiDev, &AudinAttr);
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

    if (bVqe) {
        AI_TALKVQE_CONFIG_S stAiVqeTalkAttr;
        AI_TALKVQE_CONFIG_S* pstAiVqeTalkAttr = (AI_TALKVQE_CONFIG_S*)&stAiVqeTalkAttr;

        memset(&stAiVqeTalkAttr, 0, sizeof(AI_TALKVQE_CONFIG_S));
        if (((AudinAttr.enSamplerate == AUDIO_SAMPLE_RATE_8000)
                || (AudinAttr.enSamplerate == AUDIO_SAMPLE_RATE_16000))
            && ChCnt == 2) {
            pstAiVqeTalkAttr->s32WorkSampleRate = AudinAttr.enSamplerate;
            _update_agc_anr_setting(pstAiVqeTalkAttr);
            _update_aec_setting(pstAiVqeTalkAttr);
        } else {
            printf("[error] AEC will need to setup record in to channel Count = 2\n");
            printf("[error] VQE only support on 8k/16k sample rate. current[%d]\n",
                AudinAttr.enSamplerate);
        }

        s32Ret = CVI_AI_SetTalkVqeAttr(AiDev, AiChn, 0, 0, &stAiVqeTalkAttr);
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

    if ((enType == PT_G711A || enType == PT_G711U) && (Chnsample_rate != 8000)) {
        Chnsample_rate = 8000;
        printf("G711 only support sr 8000,change to 8000.\n");
    }

    if (Chnsample_rate != (unsigned int)AudinAttr.enSamplerate) {
        s32Ret = CVI_AI_EnableReSmp(AiDev, AiChn, Chnsample_rate);
        if (s32Ret != CVI_SUCCESS) {
            printf("[error],[%s],[line:%d],\n", __func__, __LINE__);
            goto ERROR1;
        }
        bReSmp = true;
    }

    AENC_CHN_ATTR_S stAencAttr;
    _update_Aenc_setting(&AudinAttr, &stAencAttr, enType, Chnsample_rate, bVqe);

    s32Ret = CVI_AENC_CreateChn(AeChn, &stAencAttr);
    if (s32Ret != CVI_SUCCESS) {
        printf("s32Ret=%#x,\n", s32Ret);
        goto ERROR;
    }

    AEC_FRAME_S stAecFrm;
    AUDIO_FRAME_S stFrame;
    AUDIO_STREAM_S stStream;

    pstThreadAioCtx->is_running = true;
    while (pstThreadAioCtx->is_running) {
        s32Ret = CVI_AI_GetFrame(AiDev, AiChn, &stFrame, &stAecFrm, -1);
        if (s32Ret) {
            printf("[error] ai getframe error\n");
            break;
        }

        if (pstThreadAioCtx->frame_handler)
            pstThreadAioCtx->frame_handler(&stFrame);
        if (NULL == pstThreadAioCtx->stream_handler) {
            CVI_AI_ReleaseFrame(AiDev, AiChn, &stFrame, &stAecFrm);
            continue;
        }

        s32Ret = CVI_AENC_SendFrame(AeChn, &stFrame, &stAecFrm);
        if (s32Ret) {
            printf("[error] CVI_AENC_SendFrame error\n");
            break;
        }

        s32Ret = CVI_AENC_GetStream(AeChn, &stStream, 0);
        if (s32Ret != CVI_SUCCESS) {
            printf("[error],[%s],[line:%d],\n", __func__, __LINE__);
            goto ERROR;
        }
        if (!stStream.u32Len) {
            printf("stStream conitinue");
            continue;
        }

        if (pstThreadAioCtx->stream_handler)
            pstThreadAioCtx->stream_handler(&stStream);

        CVI_AENC_ReleaseStream(AeChn, &stStream);
        CVI_AI_ReleaseFrame(AiDev, AiChn, &stFrame, &stAecFrm);
    }

ERROR:
    CVI_AENC_DestroyChn(AeChn);
    _destroy_Aenc_setting(&stAencAttr);
ERROR1:
    if (bReSmp)
        CVI_AI_DisableReSmp(AiDev, AiChn);
    if (bVqe)
        CVI_AI_DisableVqe(AiDev, AiChn);
    CVI_AI_DisableChn(AiDev, AiChn);
ERROR2:
    // CVI_AI_Disable(AiDev); // Segmentation fault in thread mode
ERROR3:
    CVI_AUDIO_DEINIT();

    if (s32Ret != CVI_SUCCESS)
        return (CVI_VOID*)(uintptr_t)s32Ret;
    else
        return (CVI_VOID*)CVI_SUCCESS;
}
