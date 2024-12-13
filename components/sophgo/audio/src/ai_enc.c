static CVI_S32 app_ipcam_Audio_AencGetAttr(APP_AUDIO_CFG_S* pstAudioCfg, AENC_CHN_ATTR_S* pAencAttrs)
{
    CVI_S32 s32Ret = CVI_FAILURE;
    if (pAencAttrs == NULL || pstAudioCfg == NULL) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "Attribute NULL !!! Cannot proceed to create AENC channe!!\n");
        return s32Ret;
    }

    memset(pAencAttrs, 0, sizeof(AENC_CHN_ATTR_S));
    pAencAttrs->enType = pstAudioCfg->enAencType;
    pAencAttrs->u32BufSize = pstAudioCfg->u32FrmNum;
    pAencAttrs->u32PtNumPerFrm = pstAudioCfg->u32PtNumPerFrm;
    if (pAencAttrs->enType == PT_ADPCMA) {
        AENC_ATTR_ADPCM_S* pstAdpcmAenc = (AENC_ATTR_ADPCM_S*)malloc(sizeof(AENC_ATTR_ADPCM_S));
        if (pstAdpcmAenc == NULL) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "pstAdpcmAenc malloc failed!\n");
            return s32Ret;
        }
        pstAdpcmAenc->enADPCMType = ADPCM_TYPE_DVI4;
        pAencAttrs->pValue = (CVI_VOID*)pstAdpcmAenc;
    } else if (pAencAttrs->enType == PT_G711A || pAencAttrs->enType == PT_G711U) {
        AENC_ATTR_G711_S* pstAencG711 = (AENC_ATTR_G711_S*)malloc(sizeof(AENC_ATTR_G711_S));
        if (pstAencG711 == NULL) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "pstAencG711 malloc failed!\n");
            return s32Ret;
        }
        pAencAttrs->pValue = (CVI_VOID*)pstAencG711;
    } else if (pAencAttrs->enType == PT_G726) {
        AENC_ATTR_G726_S* pstAencG726 = (AENC_ATTR_G726_S*)malloc(sizeof(AENC_ATTR_G726_S));
        if (pstAencG726 == NULL) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "pstAdpcmAenc malloc failed!\n");
            return s32Ret;
        }
        pstAencG726->enG726bps = MEDIA_G726_32K;
        pAencAttrs->pValue = (CVI_VOID*)pstAencG726;
    } else if (pAencAttrs->enType == PT_LPCM) {
        AENC_ATTR_LPCM_S* pstAencLpcm = (AENC_ATTR_LPCM_S*)malloc(sizeof(AENC_ATTR_LPCM_S));
        if (pstAencLpcm == NULL) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "pstAdpcmAenc malloc failed!\n");
            return s32Ret;
        }
        pAencAttrs->pValue = (CVI_VOID*)pstAencLpcm;
    } else if (pAencAttrs->enType == PT_AAC) {
        APP_AUDIO_VQE_S* pstAudioVqe = &g_pstAudioCfg->astAudioVqe;
        AENC_ATTR_AAC_S* pstAencAac = (AENC_ATTR_AAC_S*)malloc(sizeof(AENC_ATTR_AAC_S));
        if (pstAencAac == NULL) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "pstAencAac malloc failed!\n");
            return s32Ret;
        }
        pstAencAac->enAACType = AAC_TYPE_AACLC;
        pstAencAac->enBitRate = AAC_BPS_32K;
        pstAencAac->enBitWidth = AUDIO_BIT_WIDTH_16;

        pstAencAac->enSmpRate = (0 != pstAudioCfg->enReSamplerate) ? pstAudioCfg->enReSamplerate : pstAudioCfg->enSamplerate;
        pstAencAac->enSoundMode = pstAudioVqe->bAiAecEnable ? AUDIO_SOUND_MODE_MONO : pstAudioCfg->enSoundmode;
        pstAencAac->enTransType = AAC_TRANS_TYPE_ADTS;
        pstAencAac->s16BandWidth = 0;
        pAencAttrs->pValue = pstAencAac;
        s32Ret = CVI_MPI_AENC_AacInit();
        APP_PROF_LOG_PRINT(LEVEL_WARN, "[cvi_info] CVI_MPI_AENC_AacInit s32Ret:%d\n", s32Ret);
    } else {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "unsupport aenc type!\n");
        return s32Ret;
    }

    return CVI_SUCCESS;
}

static CVI_VOID* Thread_AudioAenc_Proc(CVI_VOID* pArgs)
{
    if (NULL == pArgs) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "pArgs is NULL!\n");
        return NULL;
    }
    prctl(PR_SET_NAME, "Thread_Aenc_Proc", 0, 0, 0);
    CVI_S32 s32Ret = CVI_SUCCESS;
    APP_AUDIO_CFG_S* pastAudioCfg = (APP_AUDIO_CFG_S*)pArgs;
    AUDIO_STREAM_S stStream;
    // FILE* fp_rec = CVI_NULL;
    // FILE* fp_recex = CVI_NULL;

    // CVI_U32 iAencRecordTime = 0;
    // char cAencFileName[64] = { 0 };
    // snprintf(cAencFileName, sizeof(cAencFileName), AUDIOAENC_RECORD_PATH, app_ipcam_Audio_GetFileExtensions(pastAudioCfg));
    while (mAudioAencThread.bRun_flag) {
        // for test record aenc
        // if (access("/tmp/aenc", F_OK) == 0) {
        //     remove("/tmp/aenc");
        //     if (0 != pastAudioCfg->enReSamplerate) {
        //         iAencRecordTime = 10 * (pastAudioCfg->enReSamplerate / pastAudioCfg->u32PtNumPerFrm);
        //     } else {
        //         iAencRecordTime = 10 * (pastAudioCfg->enSamplerate / pastAudioCfg->u32PtNumPerFrm);
        //     }
        // }
        s32Ret = CVI_AENC_GetStream(pastAudioCfg->u32AeChn, &stStream, CVI_FALSE);
        if (s32Ret != CVI_SUCCESS) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_AENC_GetStream(%d), failed with %#x!!\n", pastAudioCfg->u32AeChn, s32Ret);
            continue;
        }

        //     if (stStream.u32Len > 0) {
        //         if (g_stAudioRecord.iStatus) {
        //             // for web record aenc
        //             if (fp_recex == NULL) {
        //                 char cAencFileName[128] = { 0 };
        //                 snprintf(cAencFileName, sizeof(cAencFileName), "%s_%d.%s",
        //                     g_stAudioRecord.cAencFileName, pastAudioCfg->enSamplerate, app_ipcam_Audio_GetFileExtensions(pastAudioCfg));
        //                 fp_recex = fopen(cAencFileName, "wb");
        //                 APP_PROF_LOG_PRINT(LEVEL_WARN, "Start %s Record AENC!!\n", cAencFileName);
        //             }
        //             if (fp_recex) {
        //                 fwrite(stStream.pStream, 1, stStream.u32Len, fp_recex);
        //             } else {
        //                 APP_PROF_LOG_PRINT(LEVEL_ERROR, "open %s failed!!\n", cAencFileName);
        //             }
        //         } else {
        //             if (fp_recex) {
        //                 APP_PROF_LOG_PRINT(LEVEL_WARN, "End Record AENC!!\n");
        //                 fclose(fp_recex);
        //                 fp_recex = NULL;
        //             }
        //         }

        //         if (iAencRecordTime) {
        //             if (fp_rec == NULL) {
        //                 fp_rec = fopen(cAencFileName, "wb");
        //                 APP_PROF_LOG_PRINT(LEVEL_WARN, "Start Record AENC!!\n");
        //             }
        //             if (fp_rec) {
        //                 fwrite(stStream.pStream, 1, stStream.u32Len, fp_rec);
        //             }
        //             iAencRecordTime--;
        //         } else {
        //             if (fp_rec) {
        //                 APP_PROF_LOG_PRINT(LEVEL_WARN, "End Record AENC!!\n");
        //                 fclose(fp_rec);
        //                 fp_rec = NULL;
        //             }
        //         }

        //         if (gst_SocketFd > 0) {
        //             s32Ret = sendto(gst_SocketFd, stStream.pStream, stStream.u32Len, 0,
        //                 (struct sockaddr*)&gst_TargetAddr, sizeof(gst_TargetAddr));
        //             if (s32Ret <= 0) {
        //                 APP_PROF_LOG_PRINT(LEVEL_WARN, "sendto failed with %#x!!\n", s32Ret);
        //             }
        //         }
        //     }

        //     s32Ret = CVI_AENC_ReleaseStream(pastAudioCfg->u32AeChn, &stStream);
        //     if (s32Ret != CVI_SUCCESS) {
        //         APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_AENC_ReleaseStream(%d), failed with %#x!!\n", pastAudioCfg->u32AeChn, s32Ret);
        //         return NULL;
        //     }
    }
    // if (fp_rec) {
    //     fclose(fp_rec);
    //     fp_rec = NULL;
    // }
    // if (fp_recex) {
    //     fclose(fp_recex);
    //     fp_recex = NULL;
    // }
    APP_PROF_LOG_PRINT(LEVEL_WARN, "Thread_AudioAenc_Proc EXIT!\n");
    return NULL;
}

static CVI_S32 app_ipcam_Audio_AencStart(APP_AUDIO_CFG_S* pstAudioCfg)
{
    if (NULL == pstAudioCfg) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "pstAudioCfg is NULL!\n");
        return -1;
    }

    if (g_stAudioStatus.bAencInit) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "Aenc Already Init!\n");
        return 0;
    }

    CVI_S32 s32Ret = CVI_SUCCESS;
    AENC_CHN_ATTR_S stAencAttr;
    s32Ret = app_ipcam_Audio_AencGetAttr(pstAudioCfg, &stAencAttr);
    if (s32Ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "app_ipcam_Audio_AencGetAttr failed!!\n");
        return s32Ret;
    }

    /* If toggle the bFileDbgMode, audio in frame will save to file after encode */
    stAencAttr.bFileDbgMode = CVI_FALSE;
    /* create aenc chn*/
    s32Ret = CVI_AENC_CreateChn(pstAudioCfg->u32AeChn, &stAencAttr);
    if (s32Ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_AENC_CreateChn(%d) failed with %#x!\n", pstAudioCfg->u32AeChn, s32Ret);
        return s32Ret;
    }

    if (stAencAttr.pValue) {
        free(stAencAttr.pValue);
        stAencAttr.pValue = NULL;
    }

    MMF_CHN_S stSrcChn, stDestChn;
    memset(&stSrcChn, 0, sizeof(stSrcChn));
    memset(&stDestChn, 0, sizeof(stDestChn));
    stSrcChn.enModId = CVI_ID_AI;
    stSrcChn.s32DevId = pstAudioCfg->u32AiDevId;
    stSrcChn.s32ChnId = pstAudioCfg->u32AiChn;
    stDestChn.enModId = CVI_ID_AENC;
    stDestChn.s32DevId = pstAudioCfg->u32AeDevId;
    stDestChn.s32ChnId = pstAudioCfg->u32AeChn;

    s32Ret = CVI_AUD_SYS_Bind(&stSrcChn, &stDestChn);
    if (s32Ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_AUD_SYS_Bind failed with %#x!\n", s32Ret);
        return s32Ret;
    }

    pthread_attr_t pthread_attr;
    pthread_attr_init(&pthread_attr);
    struct sched_param param;
    param.sched_priority = 80;
    pthread_attr_setschedpolicy(&pthread_attr, SCHED_RR);
    pthread_attr_setschedparam(&pthread_attr, &param);
    pthread_attr_setinheritsched(&pthread_attr, PTHREAD_EXPLICIT_SCHED);

    memset(&mAudioAencThread, 0, sizeof(mAudioAencThread));
    mAudioAencThread.bRun_flag = 1;

    pfp_task_entry fun_entry = Thread_AudioAenc_Proc;
    s32Ret = pthread_create(
        &mAudioAencThread.mRun_PID,
        &pthread_attr,
        fun_entry,
        (CVI_VOID*)pstAudioCfg);
    if (s32Ret) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "audio aenc pthread_create failed:0x%x\n", s32Ret);
        return CVI_FAILURE;
    }
    g_stAudioStatus.bAencInit = 1;
    APP_PROF_LOG_PRINT(LEVEL_WARN, "Start Aenc OK!\n");
    return s32Ret;
}

static CVI_S32 app_ipcam_Audio_AencStop(APP_AUDIO_CFG_S* pstAudioCfg)
{
    if (NULL == pstAudioCfg) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "pstAudioCfg is NULL!\n");
        return -1;
    }

    CVI_S32 s32Ret = CVI_SUCCESS;
    if (g_stAudioStatus.bAencInit) {
        mAudioAencThread.bRun_flag = 0;
        if (mAudioAencThread.mRun_PID != 0) {
            pthread_join(mAudioAencThread.mRun_PID, NULL);
            mAudioAencThread.mRun_PID = 0;
        }

        MMF_CHN_S stSrcChn, stDestChn;
        memset(&stSrcChn, 0, sizeof(stSrcChn));
        memset(&stDestChn, 0, sizeof(stDestChn));
        stSrcChn.enModId = CVI_ID_AI;
        stSrcChn.s32DevId = pstAudioCfg->u32AiDevId;
        stSrcChn.s32ChnId = 0;
        stDestChn.enModId = CVI_ID_AENC;
        stDestChn.s32DevId = pstAudioCfg->u32AeDevId;
        stDestChn.s32ChnId = pstAudioCfg->u32AeChn;

        s32Ret = CVI_AUD_SYS_UnBind(&stSrcChn, &stDestChn);
        if (s32Ret != CVI_SUCCESS) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_AUD_SYS_Bind failed with %#x!\n", s32Ret);
            return s32Ret;
        }

        s32Ret = CVI_AENC_DestroyChn(pstAudioCfg->u32AeChn);
        if (s32Ret != CVI_SUCCESS) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_AENC_DestroyChn(%d) failed with %#x!\n", pstAudioCfg->u32AeChn, s32Ret);
            return s32Ret;
        }

        if (pstAudioCfg->enAencType == PT_AAC) {
            CVI_MPI_AENC_AacDeInit();
        }

        g_stAudioStatus.bAencInit = 0;
        APP_PROF_LOG_PRINT(LEVEL_WARN, "Stop Aenc OK!\n");
    }

    return s32Ret;
}
