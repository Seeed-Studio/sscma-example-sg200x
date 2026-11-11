#include "video.h"

static bool is_started   = false;
static bool video_mirror = false;
static bool video_flip   = false;

/* VPSS output blocks for channels that feed the encoder. VENC keeps 3 input
 * frames in flight, so with the default 4 VPSS had one free block and the ISP
 * trigger for 2 of every 3 sensor frames was refused ("No sc-2 buffer
 * available"): every stream ran at 10 fps whatever it was configured to.
 * 6 leaves VPSS a spare block at 30 fps (measured MinFree 1). A caller's
 * blkcnt wins, e.g. to keep large-resolution pools within the ION budget. */
#define VENC_CHN_VB_BLK_NUM 6

static bool isEncoded(const video_ch_param_t* param) {
    return param != NULL && (param->format == VIDEO_FORMAT_H264 || param->format == VIDEO_FORMAT_H265);
}

/* Rate VPSS receives from the ISP: the sensor rate when the VI channel sets
 * one, else the 30 fps the shipped sensor configs run at (sensors.c). */
static int sensorFps(void) {
    APP_PARAM_VI_CTX_S* vi = app_ipcam_Vi_Param_Get();
    if (vi != NULL && vi->astChnInfo[0].f32Fps > 0) {
        return (int)vi->astChnInfo[0].f32Fps;
    }
    return 30;
}

static int setVbPool(video_ch_index_t ch, const video_ch_param_t* param) {
    APP_PARAM_SYS_CFG_S* sys = app_ipcam_Sys_Param_Get();

    if (ch >= sys->vb_pool_num) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "ch(%d) > vb_pool_num(%d)\n", ch, sys->vb_pool_num);
        return -1;
    }
    if (param == NULL) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "param is null\n");
        return -1;
    }

    APP_PARAM_VB_CFG_S* vb = &sys->vb_pool[ch];
    vb->bEnable            = 1;
    vb->width              = param->width;
    vb->height             = param->height;
    vb->fmt                = (param->format == VIDEO_FORMAT_RGB888) ? PIXEL_FORMAT_RGB_888 : PIXEL_FORMAT_NV21;
    if (param->blkcnt > 0) {
        vb->vb_blk_num = param->blkcnt;
    } else if (isEncoded(param)) {
        vb->vb_blk_num = VENC_CHN_VB_BLK_NUM;
    }

    return 0;
}

static int setGrpChn(int grp, video_ch_index_t ch, const video_ch_param_t* param) {
    APP_PARAM_VPSS_CFG_T* vpss = app_ipcam_Vpss_Param_Get();

    /* VPSS channel output width limits in VI-VPSS online mode (per-channel
     * scaler assignment: sc_d/sc_v1/sc_v2). Exceeding the limit makes the
     * kernel hand the hardware an impossible job that silently never
     * completes (the tile fallback is disabled for online input), so reject
     * it here where it is observable. */
    static const uint32_t chn_max_w[VIDEO_CH_MAX] = {1920, 2880, 1920};
    if (param->width > chn_max_w[ch]) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "chn(%d) width(%d) exceeds scaler limit(%d)\n", ch, param->width, chn_max_w[ch]);
        return -1;
    }

    if (grp >= vpss->u32GrpCnt) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "grp(%d) > u32GrpCnt(%d)\n", grp, vpss->u32GrpCnt);
        return -1;
    }
    if (ch >= VIDEO_CH_MAX) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "ch(%d) > VIDEO_CH_MAX(%d)\n", ch, VIDEO_CH_MAX);
        return -1;
    }
    if (param == NULL) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "param is null\n");
        return -1;
    }

    APP_VPSS_GRP_CFG_T* pgrp  = &vpss->astVpssGrpCfg[grp];
    pgrp->abChnEnable[ch]     = 1;
    pgrp->aAttachEn[ch]       = 1;
    VPSS_CHN_ATTR_S* vpss_chn = &pgrp->astVpssChnAttr[ch];
    vpss_chn->u32Width        = param->width;
    vpss_chn->u32Height       = param->height;
    vpss_chn->enPixelFormat   = (param->format == VIDEO_FORMAT_RGB888) ? PIXEL_FORMAT_RGB_888 : PIXEL_FORMAT_NV21;
    /* Drop frames in hardware down to the configured rate. With every enabled
     * channel of the online group set below the sensor rate, the VPSS driver
     * (osdrv online_frc) also lets the ISP skip frames above the fastest one,
     * so a lower rate saves ISP work too. Raw channels are set to their start
     * rate; runtime decreases (Camera kFps) still go through the camera
     * callback's software gate. */
    if (param->fps > 0 && param->fps < sensorFps()) {
        vpss_chn->stFrameRate.s32SrcFrameRate = sensorFps();
        vpss_chn->stFrameRate.s32DstFrameRate = param->fps;
    }

    return 0;
}

static int setVencChn(video_ch_index_t ch, const video_ch_param_t* param) {
    APP_PARAM_VENC_CTX_S* venc = app_ipcam_Venc_Param_Get();

    if (ch >= venc->s32VencChnCnt) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "ch(%d) > u32ChnCnt(%d)\n", ch, venc->s32VencChnCnt);
        return -1;
    }

    if (param == NULL) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "param is null\n");
        return -1;
    }

    PAYLOAD_TYPE_E enType = PT_JPEG;
    if (VIDEO_FORMAT_H264 == param->format) {
        enType = PT_H264;
    } else if (VIDEO_FORMAT_H265 == param->format) {
        enType = PT_H265;
    }
    app_ipcam_Param_setVencChnType(ch, enType);
    APP_VENC_CHN_CFG_S* pvchn = &venc->astVencChnCfg[ch];
    pvchn->bEnable            = 1;
    pvchn->u32Width           = param->width;
    pvchn->u32Height          = param->height;
    pvchn->u32DstFrameRate    = param->fps;
    if (isEncoded(param) && param->fps > 0) {
        /* VPSS already delivers param->fps: no further encoder frame-rate
         * control, and rate control budgets per delivered frame. */
        pvchn->u32SrcFrameRate = param->fps;
    }

    if ((VIDEO_FORMAT_RGB888 == param->format) || (VIDEO_FORMAT_NV21 == param->format)) {
        pvchn->no_need_venc = 1;
    }

    return 0;
}

int initVideo(void) {
    APP_CHK_RET(app_ipcam_Param_Load(), "load global parameter");
    video_mirror = false;
    video_flip = false;

    return 0;
}

int deinitVideo(void) {
    if (is_started) {
        int first = 0;
        int rc = app_ipcam_Venc_Stop(APP_VENC_ALL);
        if (rc != CVI_SUCCESS) { APP_PROF_LOG_PRINT(LEVEL_ERROR, "Venc Stop failed %#x\n", rc); if (!first) first = rc; }
        rc = app_ipcam_Vpss_DeInit();
        if (rc != CVI_SUCCESS) { APP_PROF_LOG_PRINT(LEVEL_ERROR, "Vpss DeInit failed %#x\n", rc); if (!first) first = rc; }
        rc = app_ipcam_Vi_DeInit();
        if (rc != CVI_SUCCESS) { APP_PROF_LOG_PRINT(LEVEL_ERROR, "Vi DeInit failed %#x\n", rc); if (!first) first = rc; }
        rc = app_ipcam_Sys_DeInit();
        if (rc != CVI_SUCCESS) { APP_PROF_LOG_PRINT(LEVEL_ERROR, "System DeInit failed %#x\n", rc); if (!first) first = rc; }
        is_started = false;
        return first;
    }
    return 0;
}

int startVideo() {
    /* Guard against double initialization - causes VPSS crash */
    if (is_started) {
        APP_PROF_LOG_PRINT(LEVEL_WARN, "startVideo() already called, skipping\n");
        return 0;
    }

    int ret = 0;

    /* init modules include <Peripheral; Sys; VI; VB; OSD; Venc; AI; Audio; etc.> */
    ret = app_ipcam_Sys_Init();
    if (ret != 0) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "init system failed with 0x%x\n", ret);
        return ret;
    }

    ret = app_ipcam_Vi_Init();
    if (ret != 0) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "init vi module failed with 0x%x\n", ret);
        app_ipcam_Sys_DeInit();
        return ret;
    }

    ret = app_ipcam_Vpss_Init();
    if (ret != 0) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "init vpss module failed with 0x%x\n", ret);
        app_ipcam_Vpss_DeInit();
        app_ipcam_Vi_DeInit();
        app_ipcam_Sys_DeInit();
        return ret;
    }

    ret = app_ipcam_Venc_Init(APP_VENC_ALL);
    if (ret != 0) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "init venc failed with 0x%x\n", ret);
        app_ipcam_Vpss_DeInit();
        app_ipcam_Vi_DeInit();
        app_ipcam_Sys_DeInit();
        return ret;
    }

    /* start video encode */
    ret = app_ipcam_Venc_Start(APP_VENC_ALL);
    if (ret != 0) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "start venc failed with 0x%x\n", ret);
        app_ipcam_Venc_Stop(APP_VENC_ALL);
        app_ipcam_Vpss_DeInit();
        app_ipcam_Vi_DeInit();
        app_ipcam_Sys_DeInit();
        return ret;
    }

    is_started = true;
    return 0;
}

int setupVideo(video_ch_index_t ch, const video_ch_param_t* param) {
    if (ch >= VIDEO_CH_MAX) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "video ch(%d) index is out of range\n", ch);
        return -1;
    }
    if (param == NULL) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "video ch(%d) param is null\n", ch);
        return -1;
    }
    if (param->format >= VIDEO_FORMAT_COUNT) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "video ch(%d) format(%d) is not support\n", ch, param->format);
        return -1;
    }

    if (setVbPool(ch, param) != 0) {
        return -1;
    }
    if (setGrpChn(0, ch, param) != 0) {
        return -1;
    }
    if (setVencChn(ch, param) != 0) {
        return -1;
    }

    return 0;
}

int registerVideoFrameHandler(video_ch_index_t ch, int index, pfpDataConsumes handler, void* pUserData) {
    app_ipcam_Venc_Consumes_Set(ch, index, handler, pUserData);
    return 0;
}

int getVideoSnsMaxRes(uint32_t* w, uint32_t* h, uint8_t* fps) {
    uint16_t w16 = 0, h16 = 0;

    if (app_ipcam_Get_SnsMaxRes(&w16, &h16, fps) != 0) {
        return -1;
    }
    if (w) *w = w16;
    if (h) *h = h16;
    return 0;
}

int setVideoMirror(bool mirror) {
    video_mirror = mirror;
    return 0;
}
int setVideoFlip(bool flip) {
    video_flip = flip;
    return 0;
}
int getVideoMirror() {
    return video_mirror;
}
int getVideoFlip() {
    return video_flip;
}
