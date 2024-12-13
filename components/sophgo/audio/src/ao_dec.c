static CVI_S32 app_ipcam_Audio_AdecStart(APP_AUDIO_CFG_S* pstAudioCfg, APP_AUDIO_VQE_S* pstAudioVqe)
{
    if (NULL == pstAudioCfg || NULL == pstAudioVqe) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "pstAudioCfg/pstAudioVqe is NULL!\n");
        return -1;
    }

    if (g_stAudioStatus.bAdecInit) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "Adec Already Init!\n");
        return 0;
    }

    CVI_S32 s32Ret = CVI_SUCCESS;

    ADEC_CHN_ATTR_S stAdecAttr;
    ADEC_ATTR_ADPCM_S stAdpcm;
    ADEC_ATTR_G711_S stAdecG711;
    ADEC_ATTR_G726_S stAdecG726;
    ADEC_ATTR_LPCM_S stAdecLpcm;
    ADEC_ATTR_AAC_S stAdecAac;

    memset(&stAdecAttr, 0, sizeof(ADEC_CHN_ATTR_S));
    stAdecAttr.s32Sample_rate = (0 != pstAudioCfg->enReSamplerate) ? pstAudioCfg->enReSamplerate : pstAudioCfg->enSamplerate;
    // rtsp occupy 1 chn
    stAdecAttr.s32ChannelNums = (pstAudioVqe->bAiAecEnable || gst_bAudioRtsp) ? 1 : pstAudioCfg->u32ChnCnt;
    stAdecAttr.s32frame_size = pstAudioCfg->u32PtNumPerFrm;
    stAdecAttr.s32BytesPerSample = 2; // 16 bits , 2 bytes per samples
    stAdecAttr.enType = pstAudioCfg->enAencType;
    stAdecAttr.u32BufSize = pstAudioCfg->u32FrmNum;
    stAdecAttr.enMode = ADEC_MODE_STREAM; /* propose use pack mode in your app */
    stAdecAttr.bFileDbgMode = CVI_FALSE;
    if (stAdecAttr.enType == PT_ADPCMA) {
        memset(&stAdpcm, 0, sizeof(ADEC_ATTR_ADPCM_S));
        stAdecAttr.pValue = &stAdpcm;
        stAdpcm.enADPCMType = ADPCM_TYPE_DVI4;
    } else if (stAdecAttr.enType == PT_G711A || stAdecAttr.enType == PT_G711U) {
        memset(&stAdecG711, 0, sizeof(ADEC_ATTR_G711_S));
        stAdecAttr.pValue = &stAdecG711;
    } else if (stAdecAttr.enType == PT_G726) {
        memset(&stAdecG726, 0, sizeof(ADEC_ATTR_G726_S));
        stAdecAttr.pValue = &stAdecG726;
        stAdecG726.enG726bps = MEDIA_G726_32K;
    } else if (stAdecAttr.enType == PT_LPCM) {
        memset(&stAdecLpcm, 0, sizeof(ADEC_ATTR_LPCM_S));
        stAdecAttr.pValue = &stAdecLpcm;
        stAdecAttr.enMode = ADEC_MODE_PACK; /* lpcm must use pack mode */
    } else if (stAdecAttr.enType == PT_AAC) {
        CVI_MPI_ADEC_AacInit();
        stAdecAac.enTransType = AAC_TRANS_TYPE_ADTS;
        stAdecAac.enSoundMode = pstAudioVqe->bAiAecEnable ? AUDIO_SOUND_MODE_MONO : pstAudioCfg->enSoundmode;
        stAdecAac.enSmpRate = (0 != pstAudioCfg->enReSamplerate) ? pstAudioCfg->enReSamplerate : pstAudioCfg->enSamplerate;
        stAdecAttr.pValue = &stAdecAac;
        stAdecAttr.enMode = ADEC_MODE_STREAM; /* aac should be stream mode */
        stAdecAttr.s32frame_size = 1024;
    }

    s32Ret = CVI_ADEC_CreateChn(pstAudioCfg->u32AdChn, &stAdecAttr);
    if (s32Ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_ADEC_CreateChn(%d) failed with %#x!\n", pstAudioCfg->u32AdChn, s32Ret);
        return s32Ret;
    }

    g_stAudioStatus.bAdecInit = 1;
    APP_PROF_LOG_PRINT(LEVEL_WARN, "Start Adec OK!\n");
    return s32Ret;
}

static CVI_S32 app_ipcam_Audio_AdecStop(APP_AUDIO_CFG_S* pstAudioCfg)
{
    if (NULL == pstAudioCfg) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "pstAudioCfg is NULL!\n");
        return -1;
    }

    CVI_S32 s32Ret = CVI_SUCCESS;

    if (g_stAudioStatus.bAdecInit) {
        s32Ret = CVI_ADEC_DestroyChn(pstAudioCfg->u32AdChn);
        if (s32Ret != CVI_SUCCESS) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_ADEC_DestroyChn(%d) failed with %#x!\n", pstAudioCfg->u32AdChn, s32Ret);
            return s32Ret;
        }

        if (pstAudioCfg->enAencType == PT_AAC) {
            CVI_MPI_ADEC_AacDeInit();
        }

        g_stAudioStatus.bAdecInit = 0;
        APP_PROF_LOG_PRINT(LEVEL_WARN, "Stop Adec OK!\n");
    }

    return s32Ret;
}