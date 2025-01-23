#include <alsa/asoundlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/file.h>
#include <unistd.h>

#include "cvi_aio.h"

#define LOCK_FILE "/tmp/cvi_audio.lock"

static CVI_S32 cvi_audio_init()
{
    CVI_AIO_DBG("%s,%d\n", __func__, __LINE__);
    int fd = open(LOCK_FILE, O_CREAT | O_RDWR, 0666);
    if (fd == -1) {
        perror("open");
        return -1;
    }
    if (flock(fd, LOCK_EX | LOCK_NB) == -1) {
        close(fd);
        CVI_AIO_DBG("%s,%d: has been locked\n", __func__, __LINE__);
        return CVI_SUCCESS;
    }

    return CVI_AUDIO_INIT();
}

static CVI_S32 cvi_audio_deinit()
{
    CVI_AIO_DBG("%s,%d\n", __func__, __LINE__);
    int fd = open(LOCK_FILE, O_RDWR, 0666);
    if (fd == -1) {
        perror("open");
        return -1;
    }

    CVI_S32 ret = CVI_AUDIO_DEINIT();

    flock(fd, LOCK_UN);
    close(fd);
    CVI_AIO_DBG("%s,%d: has been unlocked\n", __func__, __LINE__);

    return ret;
}

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
    // CVI_AIO_DBG("pstAiVqeTalkAttr:u32OpenMask[0x%x]\n", pstAiVqeTalkAttr->u32OpenMask);
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
    // CVI_AIO_DBG("pstAiVqeTalkAttr:u32OpenMask[0x%x]\n", pstAiVqeTalkAttr->u32OpenMask);
    return CVI_FALSE;
}

void cvi_ain_params(cvi_ain_t* ain)
{
    AIO_ATTR_S* pstAudinAttr = &ain->stAudinAttr;
    AI_TALKVQE_CONFIG_S* pstAiVqeTalkAttr = &ain->stAiVqeTalkAttr;

    ain->AiDev = 0;
    ain->AiChn = 0;
    ain->ch_cnt = CHANNELS;
    ain->sample_rate = SAMPLE_RATE;
    ain->bytes_per_sample = 2; // 16-bit
    ain->vqe_on = CVI_TRUE;
    ain->period_size = SAMPLE_RATE / 1000 * 10; // default 10ms

    pstAudinAttr->enSamplerate = (AUDIO_SAMPLE_RATE_E)ain->sample_rate;
    pstAudinAttr->u32ChnCnt = 1;
    pstAudinAttr->enSoundmode = (ain->ch_cnt == 1) ? AUDIO_SOUND_MODE_MONO : AUDIO_SOUND_MODE_STEREO;
    pstAudinAttr->enBitwidth = AUDIO_BIT_WIDTH_16;
    pstAudinAttr->enWorkmode = AIO_MODE_I2S_MASTER;
    pstAudinAttr->u32EXFlag = 0;
    pstAudinAttr->u32FrmNum = 10; /* only use in bind mode */
    pstAudinAttr->u32PtNumPerFrm = ain->period_size; /* sample_rate/fps */
    pstAudinAttr->u32ClkSel = 0;
    pstAudinAttr->enI2sType = AIO_I2STYPE_INNERCODEC;

    // vqe
    memset(pstAiVqeTalkAttr, 0, sizeof(AI_TALKVQE_CONFIG_S));
    pstAiVqeTalkAttr->s32WorkSampleRate = ain->sample_rate;
    _update_agc_anr_setting(pstAiVqeTalkAttr);
    _update_aec_setting(pstAiVqeTalkAttr);
}

static uint8_t _ain_inited = 0;
CVI_S32 cvi_ain_init(cvi_ain_t* ain)
{
    CVI_AIO_DBG("%s,%d: _ain_inited=%d\n", __func__, __LINE__, _ain_inited);
    if (_ain_inited)
        return CVI_SUCCESS;

    CVI_S32 AiDev = ain->AiDev;
    CVI_S32 AiChn = ain->AiChn;

    CVI_S32 s32Ret = cvi_audio_init();
    if (s32Ret != CVI_SUCCESS) {
        CVI_AIO_ERR("[error],[%s],[line:%d]cvi_audio_init\n", __func__, __LINE__);
        return s32Ret;
    }

    s32Ret = CVI_AI_SetPubAttr(AiDev, &ain->stAudinAttr);
    if (s32Ret != CVI_SUCCESS) {
        CVI_AIO_ERR("[error],[%s],[line:%d]CVI_AI_SetPubAttr\n", __func__, __LINE__);
        goto ERROR1;
    }
    s32Ret = CVI_AI_Enable(AiDev);
    if (s32Ret != CVI_SUCCESS) {
        CVI_AIO_ERR("[error],[%s],[line:%d]CVI_AI_Enable\n", __func__, __LINE__);
        goto ERROR1;
    }
    s32Ret = CVI_AI_EnableChn(AiDev, AiChn);
    if (s32Ret != CVI_SUCCESS) {
        CVI_AIO_ERR("[error],[%s],[line:%d]CVI_AI_EnableChn\n", __func__, __LINE__);
        goto ERROR2;
    }

    if (ain->vqe_on) {
        s32Ret = CVI_AI_SetTalkVqeAttr(AiDev, AiChn, 0, 0, &ain->stAiVqeTalkAttr);
        if (s32Ret != CVI_SUCCESS) {
            CVI_AIO_ERR("[error],[%s],[line:%d]CVI_AI_SetTalkVqeAttr\n", __func__, __LINE__);
            goto ERROR3;
        }
        s32Ret = CVI_AI_EnableVqe(AiDev, AiChn);
        if (s32Ret != CVI_SUCCESS) {
            CVI_AIO_ERR("[error],[%s],[line:%d]CVI_AI_EnableVqe\n", __func__, __LINE__);
            goto ERROR3;
        }
    }

    _ain_inited = 1;
    return CVI_SUCCESS;

ERROR4:
    if (ain->vqe_on) {
        CVI_AI_DisableVqe(AiDev, AiChn);
    }
ERROR3:
    CVI_AI_DisableChn(AiDev, AiChn);
ERROR2:
    CVI_AI_Disable(AiDev);
ERROR1:
    cvi_audio_deinit();

    return s32Ret;
}

CVI_S32 cvi_ain_get_frame(cvi_ain_t* ain, AUDIO_FRAME_S* pstFrame)
{
    return CVI_AI_GetFrame(ain->AiDev, ain->AiChn, pstFrame, NULL, -1);
}

CVI_S32 cvi_ain_deinit(cvi_ain_t* ain)
{
    CVI_AIO_DBG("%s,%d: _ain_inited=%d\n", __func__, __LINE__, _ain_inited);
    if (!_ain_inited)
        return CVI_SUCCESS;

    if (ain->vqe_on) {
        CVI_AI_DisableVqe(ain->AiDev, ain->AiChn);
    }
    CVI_AI_DisableChn(ain->AiDev, ain->AiChn);
    CVI_AI_Disable(ain->AiDev);
    cvi_audio_deinit();
    _ain_inited = 0;

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

void cvi_aout_params(cvi_aout_t* aout)
{
    AIO_ATTR_S* pstAudoutAttr = &aout->stAudoutAttr;

    aout->AoDev = 0;
    aout->AoChn = 0;

    aout->ch_cnt = CHANNELS;
    aout->sample_rate = SAMPLE_RATE;
    aout->bytes_per_sample = 2; // 16-bit
    aout->vqe_on = CVI_FALSE;
    aout->period_size = SAMPLE_RATE / 1000 * 10; // default 10ms

    pstAudoutAttr->enSamplerate = (AUDIO_SAMPLE_RATE_E)aout->sample_rate;
    pstAudoutAttr->u32ChnCnt = 1;
    pstAudoutAttr->enSoundmode = (aout->ch_cnt == 1) ? AUDIO_SOUND_MODE_MONO : AUDIO_SOUND_MODE_STEREO;
    pstAudoutAttr->enBitwidth = AUDIO_BIT_WIDTH_16;
    pstAudoutAttr->enWorkmode = AIO_MODE_I2S_MASTER;
    pstAudoutAttr->u32EXFlag = 0;
    pstAudoutAttr->u32FrmNum = 10; /* only use in bind mode */
    pstAudoutAttr->u32PtNumPerFrm = aout->period_size; /* sample_rate/fps */
    pstAudoutAttr->u32ClkSel = 0;
    pstAudoutAttr->enI2sType = AIO_I2STYPE_INNERCODEC;

    // vqe
    _update_down_vqe_setting(aout);
}

static uint8_t _aout_inited = 0;
CVI_S32 cvi_aout_init(cvi_aout_t* aout)
{
    CVI_AIO_DBG("%s,%d: _aout_inited=%d\n", __func__, __LINE__, _aout_inited);
    if (_aout_inited)
        return CVI_SUCCESS;

    CVI_S32 AoDev = aout->AoDev;
    CVI_S32 AoChn = aout->AoChn;

    CVI_S32 s32Ret = cvi_audio_init();
    if (s32Ret != CVI_SUCCESS) {
        CVI_AIO_ERR("[error],[%s],[line:%d]cvi_audio_init\n", __func__, __LINE__);
        return s32Ret;
    }

    s32Ret = CVI_AO_SetPubAttr(AoDev, &aout->stAudoutAttr);
    if (s32Ret != CVI_SUCCESS) {
        CVI_AIO_ERR("[cvi_error],[%s],[line:%d]CVI_AO_SetPubAttr\n", __func__, __LINE__);
        goto ERROR1;
    }
    s32Ret = CVI_AO_Enable(AoDev);
    if (s32Ret != CVI_SUCCESS) {
        CVI_AIO_ERR("[cvi_error],[%s],[line:%d]CVI_AO_Enable\n", __func__, __LINE__);
        goto ERROR1;
    }
    s32Ret = CVI_AO_EnableChn(AoDev, AoChn);
    if (s32Ret != CVI_SUCCESS) {
        CVI_AIO_ERR("[cvi_error],[%s],[line:%d]CVI_AO_EnableChn\n", __func__, __LINE__);
        goto ERROR2;
    }

    if (aout->vqe_on) {
        s32Ret = CVI_AO_SetVqeAttr(AoDev, AoChn, &aout->stVqeConfig);
        if (s32Ret != CVI_SUCCESS) {
            CVI_AIO_ERR("[cvi_error],[%s],[line:%d]CVI_AO_SetVqeAttr\n", __func__, __LINE__);
            goto ERROR3;
        }
        s32Ret = CVI_AO_EnableVqe(AoDev, AoChn);
        if (s32Ret != CVI_SUCCESS) {
            CVI_AIO_ERR("[cvi_error],[%s],[line:%d]CVI_AO_EnableVqe\n", __func__, __LINE__);
            goto ERROR3;
        }
    }

    aout->frame_size = aout->stAudoutAttr.u32PtNumPerFrm * aout->ch_cnt * aout->bytes_per_sample;
    CVI_AIO_DBG("frame_size: %d\n", aout->frame_size);
    aout->frame_buf = malloc(aout->frame_size);
    if (aout->frame_buf == NULL) {
        CVI_AIO_ERR("[cvi_error],[%s],[line:%d]malloc frame_buf failed.\n", __func__, __LINE__);
        s32Ret = CVI_FAILURE;
        goto ERROR4;
    }

    _aout_inited = 1;
    return CVI_SUCCESS;

ERROR4:
    if (aout->vqe_on) {
        CVI_AO_DisableVqe(AoDev, AoChn);
    }
ERROR3:
    CVI_AO_DisableChn(AoDev, AoChn);
ERROR2:
    CVI_AO_Disable(AoDev);
ERROR1:
    cvi_audio_deinit();

    return s32Ret;
}

CVI_S32 cvi_aout_put_frame(cvi_aout_t* aout)
{
    AUDIO_FRAME_S stFrame;
    stFrame.u64VirAddr[0] = (CVI_U8*)aout->frame_buf;
    stFrame.u32Len = aout->stAudoutAttr.u32PtNumPerFrm;
    stFrame.u64TimeStamp = 0;
    stFrame.enSoundmode = aout->stAudoutAttr.enSoundmode;
    stFrame.enBitwidth = AUDIO_BIT_WIDTH_16;

    CVI_S32 s32Ret = CVI_AO_SendFrame(aout->AoDev, aout->AoChn, &stFrame, 1000);
    if (s32Ret != CVI_SUCCESS)
        CVI_AIO_ERR("[cvi_info] CVI_AO_SendFrame failed with %#x!\n", s32Ret);

    return s32Ret;
}

CVI_S32 cvi_aout_deinit(cvi_aout_t* aout)
{
    CVI_AIO_DBG("%s,%d: _aout_inited=%d\n", __func__, __LINE__, _aout_inited);
    if (!_aout_inited)
        return CVI_SUCCESS;

    if (aout->frame_buf) {
        free(aout->frame_buf);
        aout->frame_buf = NULL;
    }
    if (aout->vqe_on) {
        CVI_AO_DisableVqe(aout->AoDev, aout->AoChn);
    }
    CVI_AO_DisableChn(aout->AoDev, aout->AoChn);
    CVI_AO_Disable(aout->AoDev);
    cvi_audio_deinit();
    _aout_inited = 0;

    return 0;
}
