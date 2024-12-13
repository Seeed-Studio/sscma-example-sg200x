static RUN_THREAD_PARAM mAudioAiThread;

static void Audio_Stereo2Mono(short* sInput, short* sOutput, int len)
{
    if ((NULL == sInput) || (sOutput == sInput) || (len <= 0)) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "param invalid!\n");
        return;
    }

    int i = 0;
    for (i = 0; i < len; i++) {
        *(sOutput + i) = *(sInput + (2 * i));
    }
}

static CVI_VOID* Thread_AudioAi_Proc(CVI_VOID* pArgs)
{
    if (NULL == pArgs) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "pArgs is NULL!\n");
        return NULL;
    }
    prctl(PR_SET_NAME, "Thread_AudioAi_Proc", 0, 0, 0);

    CVI_S32 s32Ret = CVI_SUCCESS;
    APP_AUDIO_CFG_S* pastAudioCfg = (APP_AUDIO_CFG_S*)pArgs;
    if (pastAudioCfg->u32ChnCnt == 1) {
        // chn0 bind aenc chn1 get raw frame
        APP_PROF_LOG_PRINT(LEVEL_WARN, "chn0 bind aenc can't get raw frame!\n");
        return NULL;
    }

    // FILE* fp_rec = CVI_NULL;
    CVI_U32 iAudioFactor = 0;
    // CVI_U32 iAiRecordTime = 0;
    AEC_FRAME_S stAecFrm;
    AUDIO_FRAME_S stFrame;
    AUDIO_FRAME_S stNewFrame;

    // AEC will output only one single channel with 2 channels in
    CVI_S32 s32OutputChnCnt = 1; //(pastAudioCfg->enSoundmode == AUDIO_SOUND_MODE_MONO) ? 1 : 2;
    if (pastAudioCfg->enBitwidth == AUDIO_BIT_WIDTH_16) {
        iAudioFactor = s32OutputChnCnt * 2;
    } else {
        iAudioFactor = s32OutputChnCnt * 4;
    }

    while (mAudioAiThread.bRun_flag) {
        // if (access("/tmp/audio", F_OK) == 0) {
        //     remove("/tmp/audio");
        //     iAiRecordTime = 10 * (pastAudioCfg->enSamplerate / pastAudioCfg->u32PtNumPerFrm);
        // }
        // chn0 bind aenc chn1 get raw frame
        s32Ret = CVI_AI_GetFrame(pastAudioCfg->u32AiDevId, (pastAudioCfg->u32ChnCnt - 1), &stFrame, &stAecFrm, -1);
        if (s32Ret != CVI_SUCCESS) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_AI_GetFrame None!!\n");
            continue;
        }

        if (stFrame.u32Len > 0) {
            memcpy(&stNewFrame, &stFrame, sizeof(AUDIO_FRAME_S));
            stNewFrame.u32Len = (stFrame.u32Len * iAudioFactor);
            stNewFrame.u64VirAddr[0] = malloc(stNewFrame.u32Len);
            if (NULL == stNewFrame.u64VirAddr[0]) {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "stNewFrame.u64VirAddr[0] malloc failed!\n");
                break;
            }
            if (pastAudioCfg->enSoundmode == AUDIO_SOUND_MODE_MONO) {
                memcpy(stNewFrame.u64VirAddr[0], stFrame.u64VirAddr[0], stNewFrame.u32Len);
            } else {
                // stereo to mono
                app_ipcam_Audio_Stereo2Mono((short*)stFrame.u64VirAddr[0], (short*)stNewFrame.u64VirAddr[0], stFrame.u32Len);
            }
            s32Ret = app_ipcam_LList_Data_Push(&stNewFrame, g_pAudioDataCtx);
            if (s32Ret != CVI_SUCCESS) {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "Venc streaming push linklist failed!\n");
            }
            // test code record ai auido
            // if (iAiRecordTime) {
            //     if (fp_rec == NULL) {
            //         fp_rec = fopen(AUDIOAI_RECORD_PATH, "wb");
            //         APP_PROF_LOG_PRINT(LEVEL_WARN, "Start Record AI!!\n");
            //     }
            //     if (fp_rec) {
            //         if ((stNewFrame.enBitwidth != AUDIO_BIT_WIDTH_16) && (stNewFrame.enBitwidth != AUDIO_BIT_WIDTH_32)) {
            //             APP_PROF_LOG_PRINT(LEVEL_ERROR, "Not support format bitwidth\n");
            //         } else {
            //             fwrite(stNewFrame.u64VirAddr[0], 1, stNewFrame.u32Len, fp_rec);
            //         }
            //     }
            //     iAiRecordTime--;
            // } else {
            //     if (fp_rec) {
            //         APP_PROF_LOG_PRINT(LEVEL_WARN, "End Record AI!!\n");
            //         fclose(fp_rec);
            //         fp_rec = NULL;
            //     }
            // }

            free(stNewFrame.u64VirAddr[0]);
            stNewFrame.u64VirAddr[0] = NULL;
        } else {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "block mode return size 0...\n");
        }
    }
    // if (fp_rec) {
    //     fclose(fp_rec);
    //     fp_rec = NULL;
    // }
    APP_PROF_LOG_PRINT(LEVEL_WARN, "Thread_AudioAi_Proc EXIT!\n");
    return NULL;
}

static CVI_S32 app_ipcam_Audio_AiStart(APP_AUDIO_CFG_S* pstAudioCfg, APP_AUDIO_VQE_S* pstAudioVqe)
{
    if (NULL == pstAudioCfg || NULL == pstAudioVqe) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "pstAudioCfg/pstAudioVqe is NULL!\n");
        return CVI_FAILURE;
    }

    if (g_stAudioStatus.bAiInit) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "Ai Already Init!\n");
        return CVI_SUCCESS
    }

    CVI_S32 s32Ret = CVI_SUCCESS;
    CVI_S32 i = 0;

    AIO_ATTR_S stAiAttr;
    memset(&stAiAttr, 0, sizeof(AIO_ATTR_S));
    stAiAttr.enSamplerate = pstAudioCfg->enSamplerate;
    stAiAttr.enBitwidth = pstAudioCfg->enBitwidth;
    stAiAttr.enWorkmode = pstAudioCfg->enWorkmode;
    stAiAttr.enSoundmode = pstAudioCfg->enSoundmode;
    stAiAttr.u32EXFlag = pstAudioCfg->u32EXFlag;
    stAiAttr.u32FrmNum = pstAudioCfg->u32FrmNum;
    stAiAttr.u32PtNumPerFrm = pstAudioCfg->u32PtNumPerFrm;
    stAiAttr.u32ChnCnt = pstAudioCfg->u32ChnCnt;
    stAiAttr.u32ClkSel = pstAudioCfg->u32ClkSel;
    stAiAttr.enI2sType = pstAudioCfg->enI2sType;
    s32Ret = CVI_AI_SetPubAttr(pstAudioCfg->u32AiDevId, &stAiAttr);
    if (s32Ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_AI_SetPubAttr failed with %#x!\n", s32Ret);
        return s32Ret;
    }

    for (i = 0; i <= (CVI_S32)stAiAttr.u32ChnCnt; i++) {
        s32Ret = CVI_AI_EnableChn(pstAudioCfg->u32AiDevId, i);
        if (s32Ret != CVI_SUCCESS) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_AI_EnableChn(%d) failed with %#x!\n", i, s32Ret);
            return s32Ret;
        }

        if (i == 0) {
            if (pstAudioVqe->bAiAgcEnable || pstAudioVqe->bAiAnrEnable || pstAudioVqe->bAiAecEnable) {
                AI_TALKVQE_CONFIG_S stAiVqeTalkAttr;
                AI_TALKVQE_CONFIG_S* pstAiVqeTalkAttr = (AI_TALKVQE_CONFIG_S*)&stAiVqeTalkAttr;
                memset(&stAiVqeTalkAttr, 0, sizeof(AI_TALKVQE_CONFIG_S));
                // VQE only support on 8k/16k sample rate
                if ((pstAudioCfg->enSamplerate == AUDIO_SAMPLE_RATE_8000) || (pstAudioCfg->enSamplerate == AUDIO_SAMPLE_RATE_16000)) {
                    if (pstAudioVqe->bAiAgcEnable) {
                        pstAiVqeTalkAttr->u32OpenMask |= (AGC_ENABLE | DCREMOVER_ENABLE);
                        pstAiVqeTalkAttr->stAgcCfg = pstAudioVqe->mAiAgcCfg;
                    }
                    if (pstAudioVqe->bAiAnrEnable) {
                        pstAiVqeTalkAttr->u32OpenMask |= (NR_ENABLE | DCREMOVER_ENABLE);
                        pstAiVqeTalkAttr->stAnrCfg = pstAudioVqe->mAiAnrCfg;
                    }
                    if (pstAudioVqe->bAiAecEnable) {
                        pstAiVqeTalkAttr->u32OpenMask |= (LP_AEC_ENABLE | NLP_AES_ENABLE | DCREMOVER_ENABLE);
                        pstAiVqeTalkAttr->stAecCfg = pstAudioVqe->mAiAecCfg;
                    }
                    pstAiVqeTalkAttr->s32WorkSampleRate = pstAudioCfg->enSamplerate;
                    pstAiVqeTalkAttr->para_notch_freq = 0;

                    s32Ret = CVI_AI_SetTalkVqeAttr(pstAudioCfg->u32AiDevId, i, pstAudioCfg->u32AoDevId, pstAudioCfg->u32AoChn, pstAiVqeTalkAttr);
                    if (s32Ret != CVI_SUCCESS) {
                        APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_AI_SetTalkVqeAttr failed with %#x!\n", s32Ret);
                    } else {
                        s32Ret = CVI_AI_EnableVqe(pstAudioCfg->u32AiDevId, i);
                        if (s32Ret != CVI_SUCCESS) {
                            APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_AI_EnableVqe failed with %#x!\n", s32Ret);
                        } else {
                            APP_PROF_LOG_PRINT(LEVEL_INFO, "openmask[0x%x]\n", pstAiVqeTalkAttr->u32OpenMask);
                            APP_PROF_LOG_PRINT(LEVEL_INFO, "s32WorkSampleRate[%d]\n", pstAiVqeTalkAttr->s32WorkSampleRate);
                            APP_PROF_LOG_PRINT(LEVEL_INFO, "AGC PARAM---------------------------------------------\n");
                            APP_PROF_LOG_PRINT(LEVEL_INFO, "para_agc_max_gain[%d]\n", pstAiVqeTalkAttr->stAgcCfg.para_agc_max_gain);
                            APP_PROF_LOG_PRINT(LEVEL_INFO, "para_agc_target_high[%d]\n", pstAiVqeTalkAttr->stAgcCfg.para_agc_target_high);
                            APP_PROF_LOG_PRINT(LEVEL_INFO, "para_agc_target_low[%d]\n", pstAiVqeTalkAttr->stAgcCfg.para_agc_target_low);
                            APP_PROF_LOG_PRINT(LEVEL_INFO, "ANR PARAM---------------------------------------------\n");
                            APP_PROF_LOG_PRINT(LEVEL_INFO, "para_nr_snr_coeff[%d]\n", pstAiVqeTalkAttr->stAnrCfg.para_nr_snr_coeff);
                            APP_PROF_LOG_PRINT(LEVEL_INFO, "AEC PARAM---------------------------------------------\n");
                            APP_PROF_LOG_PRINT(LEVEL_INFO, "para_aec_filter_len[%d]\n", pstAiVqeTalkAttr->stAecCfg.para_aec_filter_len);
                            APP_PROF_LOG_PRINT(LEVEL_INFO, "para_aes_std_thrd[%d]\n", pstAiVqeTalkAttr->stAecCfg.para_aes_std_thrd);
                            APP_PROF_LOG_PRINT(LEVEL_INFO, "para_aes_supp_coeff[%d]\n", pstAiVqeTalkAttr->stAecCfg.para_aes_supp_coeff);
                            APP_PROF_LOG_PRINT(LEVEL_INFO, "Start Ai VQE!\n");
                        }
                    }
                }
            }
            if (0 != pstAudioCfg->enReSamplerate) {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_AI_EnableReSmp !\n");
                s32Ret = CVI_AI_EnableReSmp(pstAudioCfg->u32AiDevId, i, pstAudioCfg->enReSamplerate);
                if (s32Ret != CVI_SUCCESS) {
                    APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_AI_EnableReSmp failed with %#x!\n", s32Ret);
                    return s32Ret;
                }
            }
        }
    }

    s32Ret = CVI_AI_Enable(pstAudioCfg->u32AiDevId);
    if (s32Ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_AI_Enable failed with %#x!\n", s32Ret);
        return s32Ret;
    }

    pthread_attr_t pthread_attr;
    pthread_attr_init(&pthread_attr);
    struct sched_param param;
    param.sched_priority = 80;
    pthread_attr_setschedpolicy(&pthread_attr, SCHED_RR);
    pthread_attr_setschedparam(&pthread_attr, &param);
    pthread_attr_setinheritsched(&pthread_attr, PTHREAD_EXPLICIT_SCHED);

    memset(&mAudioAiThread, 0, sizeof(mAudioAiThread));
    mAudioAiThread.bRun_flag = 1;
    pfp_task_entry fun_entry = Thread_AudioAi_Proc;
    s32Ret = pthread_create(
        &mAudioAiThread.mRun_PID,
        &pthread_attr,
        fun_entry,
        (CVI_VOID*)pstAudioCfg);
    if (s32Ret) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "audio ai pthread_create failed:0x%x\n", s32Ret);
        return CVI_FAILURE;
    }

    g_stAudioStatus.bAiInit = 1;
    APP_PROF_LOG_PRINT(LEVEL_WARN, "Start Ai OK!\n");
    return s32Ret;
}

static CVI_S32 app_ipcam_Audio_AiStop(APP_AUDIO_CFG_S* pstAudioCfg, APP_AUDIO_VQE_S* pstAudioVqe)
{
    if (NULL == pstAudioCfg || NULL == pstAudioVqe) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "pstAudioCfg/pstAudioVqe is NULL!\n");
        return -1;
    }

    CVI_S32 s32Ret = CVI_SUCCESS;
    CVI_S32 i = 0;
    if (g_stAudioStatus.bAiInit) {
        mAudioAiThread.bRun_flag = 0;
        if (mAudioAiThread.mRun_PID != 0) {
            pthread_join(mAudioAiThread.mRun_PID, NULL);
            mAudioAiThread.mRun_PID = 0;
        }

        for (i = 0; i <= (CVI_S32)pstAudioCfg->u32ChnCnt; i++) {
            if (i == 0) {
                if (pstAudioVqe->bAiAgcEnable || pstAudioVqe->bAiAnrEnable || pstAudioVqe->bAiAecEnable) {
                    s32Ret = CVI_AI_DisableVqe(pstAudioCfg->u32AiDevId, i);
                    if (s32Ret != CVI_SUCCESS) {
                        APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_AI_DisableVqe failed with %#x!\n", s32Ret);
                        return s32Ret;
                    }
                }
                if (0 != pstAudioCfg->enReSamplerate) {
                    s32Ret = CVI_AI_DisableReSmp(pstAudioCfg->u32AiDevId, i);
                    if (s32Ret != CVI_SUCCESS) {
                        APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_AI_DisableReSmp failed with %#x!\n", s32Ret);
                        return s32Ret;
                    }
                }
            }

            s32Ret = CVI_AI_DisableChn(pstAudioCfg->u32AiDevId, i);
            if (s32Ret != CVI_SUCCESS) {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_AI_DisableChn(%d) failed with %#x!\n", i, s32Ret);
                return s32Ret;
            }
        }

        s32Ret = CVI_AI_Disable(pstAudioCfg->u32AiDevId);
        if (s32Ret != CVI_SUCCESS) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_AI_Disable failed with %#x!\n", s32Ret);
            return s32Ret;
        }

        g_stAudioStatus.bAiInit = 0;
        APP_PROF_LOG_PRINT(LEVEL_WARN, "Stop Ai OK!\n");
    }

    return s32Ret;
}
