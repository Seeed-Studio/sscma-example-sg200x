
int app_ipcam_Audio_AoPlay(char* pAudioFile, AUDIO_AO_PLAY_TYPE_E eAoType)
{
    int s32Ret = 0;

    if (NULL == pAudioFile) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "pAudioFile is NULL!\n");
        sleep(1);
        return -1;
    }

    if (g_stAudioStatus.bAoInit == 0) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "Ao no Init!\n");
        sleep(1);
        return -1;
    }

    int flen = 0;
    int offset = 0;
    CVI_U8* pBuffer = NULL;
    AUDIO_FRAME_S stAoSendFrame;
    APP_AUDIO_CFG_S* pstAudioCfg = &g_pstAudioCfg->astAudioCfg;
    FILE* fp = NULL;

    if (AUDIO_AO_PLAY_TYPE_INTERCOM != eAoType) {
        fp = fopen(pAudioFile, "r");
        if (NULL == fp) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "fopen %s fail!\n", pAudioFile);
            return CVI_FAILURE;
        }

        fseek(fp, 0, SEEK_END);
        flen = ftell(fp);
        rewind(fp);
        if ((flen <= 0) || (flen > AUDIO_OVERSIZE)) {
            fclose(fp);
            fp = NULL;
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "Invalid audio file!size1:%d!\n", flen);
            return CVI_FAILURE;
        }

        pBuffer = (CVI_U8*)malloc(flen + 1);
        if (pBuffer == NULL) {
            fclose(fp);
            fp = NULL;
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "malloc pBuffer failed\n");
            return CVI_FAILURE;
        }
        memset(pBuffer, 0, flen + 1);
        fread(pBuffer, 1, flen, fp);
        fclose(fp);
        fp = NULL;
        APP_PROF_LOG_PRINT(LEVEL_WARN, "Open audio file!Len:%d!\n", flen);
    } else {
        if (gst_SocketFd <= 0) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "gst_SocketFd failed!\n");
            return CVI_FAILURE;
        }

        int tmpLen = (pstAudioCfg->enAencType == PT_AAC) ? 1024 : ADEC_MAX_FRAME_NUMS;
        pBuffer = (CVI_U8*)malloc(tmpLen);
        if (pBuffer == NULL) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "malloc pBuffer failed\n");
            return CVI_FAILURE;
        }
        memset(pBuffer, 0, tmpLen);
        unsigned int addrlen = sizeof(gst_TargetAddr);
        flen = recvfrom(gst_SocketFd, pBuffer, tmpLen, 0,
            (struct sockaddr*)&gst_TargetAddr, &addrlen);
        if (flen <= 0) {
            free(pBuffer);
            pBuffer = NULL;
            sleep(1);
            return CVI_FAILURE;
        }
    }

#ifdef MP3_SUPPORT
    void* pMp3DecHandler = NULL;
#endif
    if (eAoType == AUDIO_AO_PLAY_TYPE_MP3) {
#ifdef MP3_SUPPORT
        // step1:start init
        pMp3DecHandler = CVI_MP3_Decode_Init(NULL);
        if (pMp3DecHandler == NULL) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_MP3_Decode_Init failed!\n");
            return CVI_FAILURE;
        }
        // optional:user call also get the data from call back function
        s32Ret = CVI_MP3_Decode_InstallCb(pMp3DecHandler, app_ipcam_Audio_MP3CB);
        if (s32Ret != CVI_SUCCESS) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_MP3_Decode_InstallCb failed with %#x!\n", s32Ret);
            return CVI_FAILURE;
        }
#endif
    }

    do {
        if (eAoType == AUDIO_AO_PLAY_TYPE_AENC || eAoType == AUDIO_AO_PLAY_TYPE_INTERCOM) {
            if (g_stAudioStatus.bAdecInit == 0) {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "Adec no Init!\n");
                sleep(1);
                return 0;
            }
            AUDIO_STREAM_S stAudioStream;
            memset(&stAudioStream, 0, sizeof(AUDIO_STREAM_S));
            if (flen > ADEC_MAX_FRAME_NUMS) {
                if (pstAudioCfg->enAencType == PT_AAC) {
                    if (flen < 1024) {
                        stAudioStream.u32Len = flen;
                    } else {
                        stAudioStream.u32Len = 1024; // aac len must be 1024
                    }
                } else {
                    stAudioStream.u32Len = ADEC_MAX_FRAME_NUMS; // testing for g726 //hi set the adec default length= 320
                }
            } else {
                stAudioStream.u32Len = flen;
            }
            stAudioStream.pStream = pBuffer + offset;

            /* here only demo adec streaming sending mode, but pack sending mode is commended */
            s32Ret = CVI_ADEC_SendStream(pstAudioCfg->u32AdChn, &stAudioStream, CVI_TRUE);
            if (s32Ret != CVI_SUCCESS) {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_ADEC_SendStream(%d) failed with %#x!\n", pstAudioCfg->u32AdChn, s32Ret);
            }
            flen -= stAudioStream.u32Len;
            offset += stAudioStream.u32Len;

            if (((flen <= 0) || (g_stAudioPlay.iStatus == 0)) && (eAoType == AUDIO_AO_PLAY_TYPE_AENC)) {
                /*s32Ret = CVI_ADEC_ClearChnBuf(pstAudioCfg->u32AdChn);
                if (s32Ret != CVI_SUCCESS)
                {
                    APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_ADEC_ClearChnBuf failed with %#x!\n", s32Ret);
                }*/
                /* aac frame is large so sleep 3s */
                if (pstAudioCfg->enAencType == PT_AAC) {
                    sleep(3);
                    APP_PROF_LOG_PRINT(LEVEL_WARN, "End Of file play!\n");
                }
                break;
            }

            memset(&stAoSendFrame, 0x0, sizeof(AUDIO_FRAME_S));
            AUDIO_FRAME_S* pstFrame = &stAoSendFrame;
            AUDIO_FRAME_INFO_S sDecOutFrm;
            sDecOutFrm.pstFrame = (AUDIO_FRAME_S*)&stAoSendFrame;
            s32Ret = CVI_ADEC_GetFrame(pstAudioCfg->u32AdChn, &sDecOutFrm, CVI_TRUE);
            if (s32Ret != CVI_SUCCESS) {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_ADEC_GetFrame(%d) failed with %#x!\n", pstAudioCfg->u32AdChn, s32Ret);
                break;
            }

            if (pstFrame->u32Len != 0) {
                s32Ret = CVI_AO_SendFrame(pstAudioCfg->u32AoChn, pstAudioCfg->u32AdChn, pstFrame, 5000);
                if (s32Ret != CVI_SUCCESS) {
                    APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_AO_SendFrame failed with %#x!\n", s32Ret);
                }
            } else {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "dec out frame size 0\n");
            }
        } else if (eAoType == AUDIO_AO_PLAY_TYPE_RAW) {
            int iAudioLen = 0;
            memset(&stAoSendFrame, 0x0, sizeof(AUDIO_FRAME_S));
            if (flen > AO_MAX_FRAME_NUMS) {
                iAudioLen = AO_MAX_FRAME_NUMS;
            } else {
                iAudioLen = flen;
            }
            // audio is 16bit=2byte so framelen div 2
            stAoSendFrame.u32Len = iAudioLen / 2;
            stAoSendFrame.u64VirAddr[0] = pBuffer + offset;
            stAoSendFrame.u64TimeStamp = 0;
            stAoSendFrame.enSoundmode = pstAudioCfg->enSoundmode;
            stAoSendFrame.enBitwidth = pstAudioCfg->enBitwidth;
            s32Ret = CVI_AO_SendFrame(pstAudioCfg->u32AoDevId, pstAudioCfg->u32AoChn, (const AUDIO_FRAME_S*)&stAoSendFrame, 1000);
            if (s32Ret != CVI_SUCCESS) {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_AO_SendFrame failed with %#x!\n", s32Ret);
            }
            flen -= iAudioLen;
            offset += iAudioLen;
        } else if (eAoType == AUDIO_AO_PLAY_TYPE_MP3) {
#ifdef MP3_SUPPORT
            AUDIO_STREAM_S stAudioStream;
            memset(&stAudioStream, 0, sizeof(AUDIO_STREAM_S));
            if (flen > AO_MAX_FRAME_NUMS) {
                stAudioStream.u32Len = AO_MAX_FRAME_NUMS;
            } else {
                stAudioStream.u32Len = flen;
            }
            stAudioStream.pStream = pBuffer + offset;

            CVI_U32 s32OutLen = 0;
            CVI_U8* pOutPutBuffer = (CVI_U8*)malloc(stAudioStream.u32Len * 20);
            if (NULL == pOutPutBuffer) {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "pOutPutBuffer malloc failed with %#x!\n", s32Ret);
            } else {
                /* here only demo adec streaming sending mode, but pack sending mode is commended */
                memset(pOutPutBuffer, 0, stAudioStream.u32Len * 20);
                s32Ret = CVI_MP3_Decode((CVI_VOID*)pMp3DecHandler,
                    (CVI_VOID*)stAudioStream.pStream,
                    (CVI_VOID*)pOutPutBuffer,
                    (CVI_S32)stAudioStream.u32Len,
                    (CVI_S32*)&s32OutLen);
                if (s32Ret != CVI_SUCCESS) {
                    APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_MP3_Decode failed with %#x!\n", s32Ret);
                }
                if (s32OutLen > 0) {
                    if (s32OutLen > (stAudioStream.u32Len * 20)) {
                        APP_PROF_LOG_PRINT(LEVEL_ERROR, "Error outbuf size is not enough [%d] < [%d]\n", (stAudioStream.u32Len * 20), s32OutLen);
                    } else {
                        int stMP3Offset = 0;
                        do {
                            int iAudioLen = 0;
                            memset(&stAoSendFrame, 0x0, sizeof(AUDIO_FRAME_S));
                            if (s32OutLen > AO_MAX_FRAME_NUMS) {
                                iAudioLen = AO_MAX_FRAME_NUMS;
                            } else {
                                iAudioLen = s32OutLen;
                            }
                            // audio is 16bit=2byte so framelen div 2
                            stAoSendFrame.u32Len = iAudioLen / 2;
                            stAoSendFrame.u64VirAddr[0] = (pOutPutBuffer + stMP3Offset);
                            stAoSendFrame.u64TimeStamp = 0;
                            stAoSendFrame.enSoundmode = pstAudioCfg->enSoundmode;
                            stAoSendFrame.enBitwidth = pstAudioCfg->enBitwidth;
                            s32Ret = CVI_AO_SendFrame(pstAudioCfg->u32AoDevId, pstAudioCfg->u32AoChn, (const AUDIO_FRAME_S*)&stAoSendFrame, 1000);
                            if (s32Ret != CVI_SUCCESS) {
                                APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_AO_SendFrame failed with %#x!\n", s32Ret);
                            }
                            s32OutLen -= iAudioLen;
                            stMP3Offset += iAudioLen;
                        } while (s32Ret != CVI_SUCCESS || s32OutLen > 0);
                    }
                }
                flen -= stAudioStream.u32Len;
                offset += stAudioStream.u32Len;
                free(pOutPutBuffer);
                pOutPutBuffer = NULL;
            }
#endif
        } else {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "unsupport eAotype:%#x!\n", eAoType);
            s32Ret = CVI_FALSE;
        }
    } while (s32Ret != CVI_SUCCESS || flen > 0);

    if (eAoType == AUDIO_AO_PLAY_TYPE_MP3) {
#ifdef MP3_SUPPORT
        CVI_MP3_Decode_DeInit(pMp3DecHandler);
#endif
    }

    if (pBuffer) {
        free(pBuffer);
        pBuffer = NULL;
    }
    return s32Ret;
}

static CVI_VOID* Thread_AudioAo_Proc(CVI_VOID* pArgs)
{
    if (NULL == pArgs) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "pArgs is NULL!\n");
        return NULL;
    }
    prctl(PR_SET_NAME, "Thread_AudioAo_Proc", 0, 0, 0);

    APP_AUDIO_CFG_S* pastAudioCfg = (APP_AUDIO_CFG_S*)pArgs;
    while (mAudioAoThread.bRun_flag) {
        if (access("/tmp/ao", F_OK) == 0) {
            remove("/tmp/ao");
            app_ipcam_Audio_AoPlay(AUDIOAI_RECORD_PATH, AUDIO_AO_PLAY_TYPE_RAW);
        }
        if (access("/tmp/adec", F_OK) == 0) {
            remove("/tmp/adec");
            char cAencFileName[64] = { 0 };
            snprintf(cAencFileName, sizeof(cAencFileName), AUDIOAENC_RECORD_PATH, app_ipcam_Audio_GetFileExtensions(pastAudioCfg));
            app_ipcam_Audio_AoPlay(cAencFileName, AUDIO_AO_PLAY_TYPE_AENC);
        }
        if (MP3_RUNNING) {
            app_ipcam_Audio_AoPlay(AUDIOMP3_RECORD_PATH, AUDIO_AO_PLAY_TYPE_MP3);
            MP3_RUNNING = CVI_FALSE;
        }
        if (g_stAudioPlay.iStatus) {
            if (app_ipcam_Audio_AoPlay(g_stAudioPlay.cAencFileName, AUDIO_AO_PLAY_TYPE_AENC) != 0) {
                g_stAudioPlay.iStatus = 0;
            }
        }
        if (gst_SocketFd > 0) {
            app_ipcam_Audio_AoPlay(g_stAudioPlay.cAencFileName, AUDIO_AO_PLAY_TYPE_INTERCOM);
        } else {
            sleep(1);
        }
    }
    APP_PROF_LOG_PRINT(LEVEL_WARN, "Thread_AudioAo_Proc EXIT!\n");
    return NULL;
}

static CVI_S32 app_ipcam_Audio_AoStart(APP_AUDIO_CFG_S* pstAudioCfg, APP_AUDIO_VQE_S* pstAudioVqe)
{
    if (NULL == pstAudioCfg || NULL == pstAudioVqe) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "pstAudioCfg/pstAudioVqe is NULL!\n");
        return -1;
    }

    if (g_stAudioStatus.bAoInit) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "Ao Already Init!\n");
        return 0;
    }

    CVI_S32 s32Ret = CVI_SUCCESS;
    AIO_ATTR_S stAoAttr;
    memset(&stAoAttr, 0, sizeof(AIO_ATTR_S));
    stAoAttr.enSamplerate = pstAudioCfg->enSamplerate;
    stAoAttr.enBitwidth = pstAudioCfg->enBitwidth;
    stAoAttr.enWorkmode = pstAudioCfg->enWorkmode;
    stAoAttr.enSoundmode = pstAudioVqe->bAiAecEnable ? AUDIO_SOUND_MODE_MONO : pstAudioCfg->enSoundmode;
    stAoAttr.u32EXFlag = pstAudioCfg->u32EXFlag;
    stAoAttr.u32FrmNum = pstAudioCfg->u32FrmNum;
    stAoAttr.u32PtNumPerFrm = pstAudioCfg->u32PtNumPerFrm;
    stAoAttr.u32ChnCnt = pstAudioVqe->bAiAecEnable ? 1 : pstAudioCfg->u32ChnCnt;
    stAoAttr.u32ClkSel = pstAudioCfg->u32ClkSel;
    stAoAttr.enI2sType = pstAudioCfg->enI2sType;

// Currently not support ao vqe
#if 0
    if (pstAudioVqe->bAoAgcEnable || pstAudioVqe->bAoAnrEnable)
    {
        AO_VQE_CONFIG_S stAoVqeAttr;
        AO_VQE_CONFIG_S *pstAoVqeAttr = (AO_VQE_CONFIG_S *)&stAoVqeAttr;
        memset(pstAoVqeAttr, 0, sizeof(AO_VQE_CONFIG_S));
        //VQE only support on 8k/16k sample rate
        if ((pstAudioCfg->enSamplerate == AUDIO_SAMPLE_RATE_8000) ||
            (pstAudioCfg->enSamplerate == AUDIO_SAMPLE_RATE_16000))
        {
            if (pstAudioVqe->bAiAgcEnable)
            {
                pstAoVqeAttr->u32OpenMask |= AO_VQE_MASK_AGC;
                pstAoVqeAttr->stAgcCfg    = pstAudioVqe->mAoAgcCfg;
            }
            if (pstAudioVqe->bAiAnrEnable)
            {
                pstAoVqeAttr->u32OpenMask |= AO_VQE_MASK_ANR;
                pstAoVqeAttr->stAnrCfg    = pstAudioVqe->mAiAnrCfg;
            }
            pstAoVqeAttr->s32FrameSample    = pstAudioCfg->u32PtNumPerFrm;
            pstAoVqeAttr->s32WorkSampleRate = pstAudioCfg->enSamplerate;

            s32Ret = CVI_AO_SetVqeAttr(pstAudioCfg->u32AoDevId, pstAudioCfg->u32AoChn, pstAoVqeAttr);
            if (s32Ret != CVI_SUCCESS)
            {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_AO_SetVqeAttr failed with %#x!\n", s32Ret);
            }
            else
            {
                s32Ret = CVI_AO_EnableVqe(pstAudioCfg->u32AoDevId, pstAudioCfg->u32AoChn);
                if (s32Ret != CVI_SUCCESS)
                {
                    APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_AO_EnableVqe failed with %#x!\n", s32Ret);
                }
                else
                {
                    APP_PROF_LOG_PRINT(LEVEL_INFO, "openmask[0x%x]\n", pstAoVqeAttr->u32OpenMask);
                    APP_PROF_LOG_PRINT(LEVEL_INFO, "s32WorkSampleRate[%d]\n", pstAoVqeAttr->s32WorkSampleRate);
                    APP_PROF_LOG_PRINT(LEVEL_INFO, "s32FrameSample[%d]\n", pstAoVqeAttr->s32FrameSample);
                    APP_PROF_LOG_PRINT(LEVEL_INFO, "AGC PARAM---------------------------------------------\n");
                    APP_PROF_LOG_PRINT(LEVEL_INFO, "para_agc_max_gain[%d]\n", pstAoVqeAttr->stAgcCfg.para_agc_max_gain);
                    APP_PROF_LOG_PRINT(LEVEL_INFO, "para_agc_target_high[%d]\n", pstAoVqeAttr->stAgcCfg.para_agc_target_high);
                    APP_PROF_LOG_PRINT(LEVEL_INFO, "para_agc_target_low[%d]\n", pstAoVqeAttr->stAgcCfg.para_agc_target_low);
                    APP_PROF_LOG_PRINT(LEVEL_INFO, "para_agc_vad_enable[%d]\n", pstAoVqeAttr->stAgcCfg.para_agc_vad_enable);
                    APP_PROF_LOG_PRINT(LEVEL_INFO, "para_agc_vad_cnt[%d]\n", pstAoVqeAttr->stAgcCfg.para_agc_vad_cnt);
                    APP_PROF_LOG_PRINT(LEVEL_INFO, "para_agc_cut6_enable[%d]\n", pstAoVqeAttr->stAgcCfg.para_agc_cut6_enable);
                    APP_PROF_LOG_PRINT(LEVEL_INFO, "ANR PARAM---------------------------------------------\n");
                    APP_PROF_LOG_PRINT(LEVEL_INFO, "para_nr_snr_coeff[%d]\n", pstAoVqeAttr->stAnrCfg.para_nr_snr_coeff);
                    APP_PROF_LOG_PRINT(LEVEL_INFO, "para_nr_noise_coeff[%d]\n", pstAoVqeAttr->stAnrCfg.para_nr_noise_coeff);
                    APP_PROF_LOG_PRINT(LEVEL_INFO, "Start AO VQE!\n");
                }
            }
        }
    }
#endif

    s32Ret = CVI_AO_SetPubAttr(pstAudioCfg->u32AoDevId, &stAoAttr);
    if (s32Ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_AO_SetPubAttr(%d) failed with %#x!\n", pstAudioCfg->u32AoDevId, s32Ret);
        return s32Ret;
    }

    s32Ret = CVI_AO_Enable(pstAudioCfg->u32AoDevId);
    if (s32Ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_AO_Enable(%d) failed with %#x!\n", pstAudioCfg->u32AoDevId, s32Ret);
        return s32Ret;
    }

    s32Ret = CVI_AO_EnableChn(pstAudioCfg->u32AoDevId, pstAudioCfg->u32AoChn);
    if (s32Ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_AO_EnableChn(%d) failed with %#x!\n", pstAudioCfg->u32AoChn, s32Ret);
        return s32Ret;
    }

    if (0 != pstAudioCfg->enReSamplerate) {
        s32Ret = CVI_AO_EnableReSmp(pstAudioCfg->u32AoDevId, pstAudioCfg->u32AoChn, pstAudioCfg->enReSamplerate);
        if (s32Ret != CVI_SUCCESS) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_AO_EnableReSmp(%d) failed with %#x!\n", pstAudioCfg->u32AoChn, s32Ret);
            return s32Ret;
        }
    }
    pthread_attr_t pthread_attr;
    pthread_attr_init(&pthread_attr);
    struct sched_param param;
    param.sched_priority = 80;
    pthread_attr_setschedpolicy(&pthread_attr, SCHED_RR);
    pthread_attr_setschedparam(&pthread_attr, &param);
    pthread_attr_setinheritsched(&pthread_attr, PTHREAD_EXPLICIT_SCHED);

    memset(&mAudioAoThread, 0, sizeof(mAudioAoThread));
    mAudioAoThread.bRun_flag = 1;

    pfp_task_entry fun_entry = Thread_AudioAo_Proc;
    s32Ret = pthread_create(
        &mAudioAoThread.mRun_PID,
        &pthread_attr,
        fun_entry,
        (CVI_VOID*)pstAudioCfg);
    if (s32Ret) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "audio ao pthread_create failed:0x%x\n", s32Ret);
        return CVI_FAILURE;
    }

    g_stAudioStatus.bAoInit = 1;
    APP_PROF_LOG_PRINT(LEVEL_WARN, "Start AO OK!\n");
    return CVI_SUCCESS;
}

static CVI_S32 app_ipcam_Audio_AoStop(APP_AUDIO_CFG_S* pstAudioCfg, APP_AUDIO_VQE_S* pstAudioVqe)
{
    if (NULL == pstAudioCfg || NULL == pstAudioVqe) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "pstAudioCfg/pstAudioVqe is NULL!\n");
        return -1;
    }

    CVI_S32 s32Ret = CVI_SUCCESS;
    if (g_stAudioStatus.bAoInit) {
        mAudioAoThread.bRun_flag = 0;
        if (mAudioAoThread.mRun_PID != 0) {
            pthread_join(mAudioAoThread.mRun_PID, NULL);
            mAudioAoThread.mRun_PID = 0;
        }

#if 0
        if (pstAudioVqe->bAoAgcEnable || pstAudioVqe->bAoAnrEnable)
        {
            s32Ret = CVI_AO_DisableVqe(pstAudioCfg->u32AoDevId, pstAudioCfg->u32AoChn);
            if (s32Ret != CVI_SUCCESS)
            {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_AO_DisableVqe failed with %#x!\n", s32Ret);
                return s32Ret;
            }
        }
#endif

        if (0 != pstAudioCfg->enReSamplerate) {
            s32Ret = CVI_AO_DisableReSmp(pstAudioCfg->u32AoDevId, pstAudioCfg->u32AoChn);
            if (s32Ret != CVI_SUCCESS) {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_AO_DisableReSmp failed with %#x!\n", s32Ret);
                return s32Ret;
            }
        }

        s32Ret = CVI_AO_DisableChn(pstAudioCfg->u32AoDevId, pstAudioCfg->u32AoChn);
        if (s32Ret != CVI_SUCCESS) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_AO_DisableChn(%d) failed with %#x!\n", pstAudioCfg->u32AoChn, s32Ret);
            return s32Ret;
        }

        s32Ret = CVI_AO_Disable(pstAudioCfg->u32AoDevId);
        if (s32Ret != CVI_SUCCESS) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_AO_Disable(%d) failed with %#x!\n", pstAudioCfg->u32AoDevId, s32Ret);
            return s32Ret;
        }

        g_stAudioStatus.bAoInit = 0;
        APP_PROF_LOG_PRINT(LEVEL_WARN, "Stop Ao OK!\n");
    }

    return CVI_SUCCESS;
}