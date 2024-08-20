#include <iostream>
#include <signal.h>
#include <syslog.h>
#include <unistd.h>

#include "rtsp_demo.h"
#include "video.h"

static CVI_VOID app_ipcam_ExitSig_handle(CVI_S32 signo)
{
    signal(SIGINT, SIG_IGN);
    signal(SIGTERM, SIG_IGN);

    if ((SIGINT == signo) || (SIGTERM == signo)) {
        deinitVideo();
        deinitRtsp();
        APP_PROF_LOG_PRINT(LEVEL_INFO, "ipcam receive a signal(%d) from terminate\n", signo);
    }

    exit(-1);
}

int fpSaveRgb888Frame(void* pData, void* pArgs)
{
#if 0
    static uint32_t count = 0;

    video_frame_t* frame = (video_frame_t*)pData;
    if (NULL == frame) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "frame is NULL\n");
        return CVI_FAILURE;
    }
    APP_PROF_LOG_PRINT(LEVEL_INFO, "Addr=0x%x, size=%u, frame=%u, timestamp=%u\n",
        frame->pdata, frame->size, frame->id, frame->timestamp);

    FILE* fp;
    char name[32] = "";
    snprintf(name, sizeof(name), "/mnt/sd/%d_%dx%d.rgb",
        count++, frame->width, frame->height);

    if (count >= 10) {
        count = 0;
    }

    APP_PROF_LOG_PRINT(LEVEL_INFO, "save frame to %s\n", name);

    fp = fopen(name, "w");
    if (fp == CVI_NULL) {
        CVI_TRACE_LOG(CVI_DBG_ERR, "open data file error\n");
        return CVI_FAILURE;
    }

    if (fwrite(frame->pdata, frame->size, 1, fp) <= 0) {
        CVI_TRACE_LOG(CVI_DBG_ERR, "fwrite data(%d) error\n", frame->id);
    }

    fclose(fp);
#endif

    return CVI_SUCCESS;
}

int fpStreamingSaveToFlash(void *pData, void *pArgs, void *pUserData)
{
    VENC_STREAM_S *pstStream = (VENC_STREAM_S *)pData;

    APP_DATA_CTX_S *pstDataCtx = (APP_DATA_CTX_S *)pArgs;
    APP_DATA_PARAM_S *pstDataParam = &pstDataCtx->stDataParam;
    APP_VENC_CHN_CFG_S *pstVencChnCfg = (APP_VENC_CHN_CFG_S *)pstDataParam->pParam;

    const char *path = pUserData ? (char *)pUserData : "/mnt/sd";

    /* dynamic touch a file: /tmp/rec then start save sreaming to flash or SD Card */
    if (access("/tmp/rec", F_OK) == 0) {
        if (pstVencChnCfg->pFile == NULL) {
            char szPostfix[8] = {0};
            char szFilePath[100] = {0};

            app_ipcam_Postfix_Get(pstVencChnCfg->enType, szPostfix);
            sprintf(szFilePath, "%s/Venc%d_idx_%d%s", path, pstVencChnCfg->VencChn, pstVencChnCfg->frameNum++, szPostfix);
            APP_PROF_LOG_PRINT(LEVEL_INFO, "start save file to %s\n", szFilePath);
            pstVencChnCfg->pFile = fopen(szFilePath, "wb");
            if (pstVencChnCfg->pFile == NULL) {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "open file err, %s\n", szFilePath);
                return CVI_FAILURE;
            }
        }

        VENC_PACK_S *ppack;
        /* just from I-frame saving */
        if ((pstVencChnCfg->enType == PT_H264) || (pstVencChnCfg->enType == PT_H265)) {
            if (pstVencChnCfg->fileNum == 0 && pstStream->u32PackCount < 2)
                return CVI_SUCCESS;
        }
        for (CVI_U32 i = 0; i < pstStream->u32PackCount; i++) {
            ppack = &pstStream->pstPack[i];
            fwrite(ppack->pu8Addr + ppack->u32Offset,
                    ppack->u32Len - ppack->u32Offset, 1, pstVencChnCfg->pFile);

            APP_PROF_LOG_PRINT(LEVEL_DEBUG, "pack[%d], PTS = %lu, Addr = %p, Len = 0x%X, Offset = 0x%X DataType=%d\n",
                    i, ppack->u64PTS, ppack->pu8Addr, ppack->u32Len, ppack->u32Offset, ppack->DataType.enH265EType);
        }

        if (pstVencChnCfg->enType == PT_JPEG) {
            fclose(pstVencChnCfg->pFile);
            pstVencChnCfg->pFile = NULL;
            APP_PROF_LOG_PRINT(LEVEL_INFO, "End save! \n");
            remove("/tmp/rec");

        } else {
            if (++pstVencChnCfg->fileNum > pstVencChnCfg->u32Duration) {
                pstVencChnCfg->fileNum = 0;
                fclose(pstVencChnCfg->pFile);
                pstVencChnCfg->pFile = NULL;
                APP_PROF_LOG_PRINT(LEVEL_INFO, "End save! \n");
                remove("/tmp/rec");
            }
        }
    }

    return CVI_SUCCESS;
}

int main(int argc, char* argv[])
{
    signal(SIGINT, app_ipcam_ExitSig_handle);
    signal(SIGTERM, app_ipcam_ExitSig_handle);

    initVideo();

    video_ch_param_t param;

    // rtsp0
    param.format = VIDEO_FORMAT_JPEG;
    param.width = 1920;
    param.height = 1080;
    param.fps = 5;
    setupVideo(VIDEO_CH0, &param);
    registerVideoFrameHandler(VIDEO_CH0, 0, fpStreamingSaveToFlash, (void*)"/mnt/sd");

    // rtsp1
    param.format = VIDEO_FORMAT_H264;
    param.width = 1920;
    param.height = 1080;
    param.fps = 30;
    setupVideo(VIDEO_CH1, &param);
    registerVideoFrameHandler(VIDEO_CH1, 0, fpStreamingSendToRtsp, NULL);

    param.format = VIDEO_FORMAT_H265;
    param.width = 1280;
    param.height = 720;
    param.fps = 15;
    setupVideo(VIDEO_CH2, &param);
    registerVideoFrameHandler(VIDEO_CH2, 0, fpStreamingSendToRtsp, NULL);

    initRtsp((0x01 << VIDEO_CH1) | (0x01 << VIDEO_CH2));
    startVideo();

    while (1) {
        sleep(1);
    };

    return 0;
}