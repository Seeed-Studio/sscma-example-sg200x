#include "video.h"

static bool is_started = false;

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

    return 0;
}

static int setGrpChn(int grp, video_ch_index_t ch, const video_ch_param_t* param) {
    APP_PARAM_VPSS_CFG_T* vpss = app_ipcam_Vpss_Param_Get();

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

    if ((VIDEO_FORMAT_RGB888 == param->format) || (VIDEO_FORMAT_NV21 == param->format)) {
        pvchn->no_need_venc = 1;
    }

    return 0;
}

int initVideo(void) {
    APP_CHK_RET(app_ipcam_Param_Load(), "load global parameter");

    return 0;
}

int deinitVideo(void) {
    if (is_started) {
        APP_CHK_RET(app_ipcam_Venc_Stop(APP_VENC_ALL), "Venc Stop");
        APP_CHK_RET(app_ipcam_Vpss_DeInit(), "Vpss DeInit");
        APP_CHK_RET(app_ipcam_Vi_DeInit(), "Vi DeInit");
        APP_CHK_RET(app_ipcam_Sys_DeInit(), "System DeInit");
        is_started = false;
    }
}

int startVideo(void) {
    /* init modules include <Peripheral; Sys; VI; VB; OSD; Venc; AI; Audio; etc.> */
    APP_CHK_RET(app_ipcam_Sys_Init(), "init systerm");
    APP_CHK_RET(app_ipcam_Vi_Init(), "init vi module");
    APP_CHK_RET(app_ipcam_Vpss_Init(), "init vpss module");
    APP_CHK_RET(app_ipcam_Venc_Init(APP_VENC_ALL), "init video encode");

    /* start video encode */
    APP_CHK_RET(app_ipcam_Venc_Start(APP_VENC_ALL), "start video processing");

    is_started = true;
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

    setVbPool(ch, param);
    setGrpChn(0, ch, param);
    setVencChn(ch, param);

    return 0;
}

int registerVideoFrameHandler(video_ch_index_t ch, int index, pfpDataConsumes handler, void* pUserData) {
    app_ipcam_Venc_Consumes_Set(ch, index, handler, pUserData);
    return 0;
}