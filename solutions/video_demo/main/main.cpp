#include <iostream>
#include <signal.h>
#include <syslog.h>
#include <unistd.h>

#include "video.h"
#include "rtsp_demo.h"

static CVI_VOID app_ipcam_ExitSig_handle(CVI_S32 signo)
{
    signal(SIGINT, SIG_IGN);
    signal(SIGTERM, SIG_IGN);

    if ((SIGINT == signo) || (SIGTERM == signo)) {
        deinitVideo();
        app_ipcam_rtsp_Server_Destroy();
        APP_PROF_LOG_PRINT(LEVEL_INFO, "ipcam receive a signal(%d) from terminate\n", signo);
    }

    exit(-1);
}

int fpSaveRgb888Frame(void* pData, void* pArgs)
{
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

    return CVI_SUCCESS;
}

int main(int argc, char* argv[])
{
    signal(SIGINT, app_ipcam_ExitSig_handle);
    signal(SIGTERM, app_ipcam_ExitSig_handle);

    initVideo();

    loadRtspParam(PARAM_CFG_INI, app_ipcam_Rtsp_Param_Get());

    // rtsp0
    video_ch_param_t param;
    param.format = VIDEO_FORMAT_H265;
    param.width = 1920;
    param.height = 1080;
    param.fps = 30;
    setupVideo(VIDEO_CH0, &param);
    registerVideoFrameHandler(VIDEO_CH0, 0, fpStreamingSendToRtsp);

    // rtsp1
    param.format = VIDEO_FORMAT_H264;
    param.width = 1280;
    param.height = 720;
    param.fps = 15;
    setupVideo(VIDEO_CH1, &param);
    registerVideoFrameHandler(VIDEO_CH1, 0, fpStreamingSendToRtsp);

    app_ipcam_Rtsp_Server_Create();

    param.format = VIDEO_FORMAT_RGB888;
    param.width = 1920;
    param.height = 1080;
    param.fps = 1;
    setupVideo(VIDEO_CH2, &param);
    registerVideoFrameHandler(VIDEO_CH2, 0, fpSaveRgb888Frame);

    startVideo();

    while (1) {
        sleep(1);
    };

    return 0;
}