#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "app_ipcam_paramparse.h"

// SYS - vb_pool
static const APP_PARAM_VB_CFG_S vbpool = {
    .bEnable = 1,
    .width = 1920,
    .height = 1080,
    .fmt = PIXEL_FORMAT_NV21,
    .enBitWidth = DATA_BITWIDTH_8,
    .enCmpMode = COMPRESS_MODE_NONE,
    .vb_blk_num = 2,
};

// VI
static const APP_PARAM_SNS_CFG_T sns_cfg_ov5647 = {
    .s32SnsId = 0,
    .enSnsType = SENSOR_OV_OV5647,
    .s32Framerate = 30,
    .s32BusId = 2,
    .s32I2cAddr = 36,
    .MipiDev = 0,
    .as16LaneId = { 2, 0, 3, -1, -1 },
    .as8PNSwap = { 0, 0, 0, 0, 0 },
    .bMclkEn = 1,
    .u8Mclk = 0,
    .u8Orien = 0,
    .bHwSync = 0,
    .u8UseDualSns = 0,
};

static const APP_PARAM_DEV_CFG_T dev_cfg = {
    .ViDev = 0,
    .enWDRMode = WDR_MODE_NONE,
};

static const APP_PARAM_PIPE_CFG_T pipe_cfg = {
    .aPipe = { 0, -1, -1, -1 },
    .enMastPipeMode = VI_OFFLINE_VPSS_OFFLINE,
};

static const APP_PARAM_CHN_CFG_T chn_cfg = {
    .s32ChnId = 0,
    .u32Width = 1920,
    .u32Height = 1080,
    .f32Fps = -1,
    .enPixFormat = PIXEL_FORMAT_NV21,
    .enDynamicRange = DYNAMIC_RANGE_SDR8,
    .enVideoFormat = VIDEO_FORMAT_LINEAR,
    .enCompressMode = COMPRESS_MODE_TILE,
};

// VPSS
static const VPSS_CHN_ATTR_S chn_attr = {
    .u32Width = 1920,
    .u32Height = 1080,
    .enVideoFormat = VIDEO_FORMAT_LINEAR,
    .enPixelFormat = PIXEL_FORMAT_NV21,
    .stFrameRate = {
        .s32SrcFrameRate = -1,
        .s32DstFrameRate = -1,
    },
    .bMirror = 0,
    .bFlip = 0,
    .u32Depth = 0,
    .stAspectRatio = {
        .enMode = ASPECT_RATIO_NONE,
    },
};

static const VPSS_GRP_ATTR_S grp_attr = {
    .u32MaxW = 1920,
    .u32MaxH = 1080,
    .enPixelFormat = PIXEL_FORMAT_NV21,
    .stFrameRate = {
        .s32SrcFrameRate = -1,
        .s32DstFrameRate = -1,
    },
    .u8VpssDev = 1,
};

static const APP_VPSS_GRP_CFG_T vpss_grp = {
    .VpssGrp = 0,
    .bEnable = 1,
    .bBindMode = 0,
    .stVpssGrpCropInfo = {
        .bEnable = 0,
    },
};

// VENC - H264
static const APP_GOP_PARAM_U h264_stNormalP = {
    .stNormalP.s32IPQpDelta = 2,
};

static const APP_RC_PARAM_S h264_stRcParam = {
    .s32FirstFrameStartQp = 35,
    .s32InitialDelay = 1000,
    .u32ThrdLv = 2,
    .s32ChangePos = 75,
    .u32MinIprop = 1,
    .u32MaxIprop = 10,
    .s32MaxReEncodeTimes = 0,
    .s32MinStillPercent = 10,
    .u32MaxStillQP = 38,
    .u32MinStillPSNR = 0,
    .u32MaxQp = 51,
    .u32MinQp = 20,
    .u32MaxIQp = 51,
    .u32MinIQp = 20,
    .u32MotionSensitivity = 24,
    .s32AvbrFrmLostOpen = 0,
    .s32AvbrFrmGap = 1,
    .s32AvbrPureStillThr = 4,
};

static const APP_VENC_CHN_CFG_S venc_h264 = {
    .bEnable = 1,
    .enType = PT_H264,
    .u32Duration = 75,
    .u32Width = 1920,
    .u32Height = 1080,
    .u32SrcFrameRate = -1,
    .u32DstFrameRate = -1,
    .u32BitRate = 1000,
    .u32MaxBitRate = 1000,
    .u32StreamBufSize = (512 << 10),
    .VpssGrp = 0,
    .VpssChn = 0,
    .u32Profile = 0,
    .bSingleCore = 0,
    .u32Gop = 50,
    .u32IQp = 38,
    .u32PQp = 38,
    .statTime = 2,
    .enBindMode = VENC_BIND_VPSS,
    .astChn[0] = {
        // src
        .enModId = CVI_ID_VPSS,
        .s32DevId = 0,
        .s32ChnId = 0,
    },
    .astChn[1] = {
        // dst
        .enModId = CVI_ID_VENC,
        .s32DevId = 0,
        .s32ChnId = 0,
    },
    .enGopMode = VENC_GOPMODE_NORMALP,
    .unGopParam = h264_stNormalP,
    .enRcMode = VENC_RC_MODE_H264CBR,
    .stRcParam = h264_stRcParam,
};

// VENC - H265
static const APP_GOP_PARAM_U h265_stNormalP = {
    .stNormalP.s32IPQpDelta = 2,
};

static const APP_RC_PARAM_S h265_stRcParam = {
    .s32FirstFrameStartQp = 35,
    .s32InitialDelay = 1000,
    .u32ThrdLv = 2,
    .s32ChangePos = 75,
    .u32MinIprop = 1,
    .u32MaxIprop = 100,
    .s32MaxReEncodeTimes = 0,
    .s32MinStillPercent = 10,
    .u32MaxStillQP = 33,
    .u32MinStillPSNR = 0,
    .u32MaxQp = 51,
    .u32MinQp = 20,
    .u32MaxIQp = 51,
    .u32MinIQp = 20,
    .u32MotionSensitivity = 24,
    .s32AvbrFrmLostOpen = 0,
    .s32AvbrFrmGap = 1,
    .s32AvbrPureStillThr = 4,
};

static const APP_VENC_CHN_CFG_S venc_h265 = {
    .bEnable = 1,
    .enType = PT_H265,
    .u32Duration = 75,
    .u32Width = 1920,
    .u32Height = 1080,
    .u32SrcFrameRate = -1,
    .u32DstFrameRate = -1,
    .u32BitRate = 3000,
    .u32MaxBitRate = 3000,
    .u32StreamBufSize = (1024 << 10),
    .VpssGrp = 0,
    .VpssChn = 0,
    .u32Profile = 0,
    .bSingleCore = 0,
    .u32Gop = 50,
    .u32IQp = 38,
    .u32PQp = 38,
    .statTime = 2,
    .enBindMode = VENC_BIND_VPSS,
    .astChn[0] = {
        // src
        .enModId = CVI_ID_VPSS,
        .s32DevId = 0,
        .s32ChnId = 0,
    },
    .astChn[1] = {
        // dst
        .enModId = CVI_ID_VENC,
        .s32DevId = 0,
        .s32ChnId = 0,
    },
    .enGopMode = VENC_GOPMODE_NORMALP,
    .unGopParam = h265_stNormalP,
    .enRcMode = VENC_RC_MODE_H265CBR,
    .stRcParam = h265_stRcParam,
};

static const APP_VENC_CHN_CFG_S venc_jpeg = {
    .bEnable = 1,
    .enType = PT_JPEG,
    .u32Duration = 0,
    .u32Width = 1920,
    .u32Height = 1080,
    .VpssGrp = 0,
    .VpssChn = 0,
    .stJpegCodecParam = {
        .quality = 20,
        .MCUPerECS = 0,
    },
    .enBindMode = VENC_BIND_DISABLE,
    .enRcMode = VENC_RC_MODE_MJPEGCBR,
    .u32StreamBufSize = (512 << 10),
};

static const APP_VENC_ROI_CFG_S roi_cfg = { 0 };

int app_ipcam_Param_Load(void)
{
    // sys
    APP_PARAM_SYS_CFG_S* sys = app_ipcam_Sys_Param_Get();
    sys->vb_pool[0] = vbpool;
    sys->vb_pool[1] = vbpool;
    sys->vb_pool[2] = vbpool;
    sys->vb_pool_num = 3;
    sys->stVIVPSSMode.aenMode[0] = VI_OFFLINE_VPSS_ONLINE;
    sys->stVPSSMode.enMode = VPSS_MODE_DUAL;
    sys->stVPSSMode.aenInput[0] = VPSS_INPUT_MEM;
    sys->stVPSSMode.aenInput[1] = VPSS_INPUT_ISP;
    sys->stVPSSMode.ViPipe[0] = 0;
    sys->stVPSSMode.ViPipe[1] = 0;
    sys->bSBMEnable = 0;

    // vi
    APP_PARAM_VI_CTX_S* vi = app_ipcam_Vi_Param_Get();
    vi->u32WorkSnsCnt = 1;
    vi->astSensorCfg[0] = sns_cfg_ov5647;
    vi->astSensorCfg[0].s32SnsId = 0;
    vi->astDevInfo[0] = dev_cfg;
    vi->astDevInfo[0].ViDev = 0;
    vi->astPipeInfo[0] = pipe_cfg;
    vi->astChnInfo[0] = chn_cfg;
    vi->astChnInfo[0].s32ChnId = 0;
    vi->astIspCfg[0].bAfFliter = 0;

    // vpss
    APP_PARAM_VPSS_CFG_T* vpss = app_ipcam_Vpss_Param_Get();
    vpss->u32GrpCnt = 1;
    vpss->astVpssGrpCfg[0] = vpss_grp;
    APP_VPSS_GRP_CFG_T* pgrp = &vpss->astVpssGrpCfg[0];
    pgrp->VpssGrp = 0;
    pgrp->stVpssGrpAttr = grp_attr;
    pgrp->stVpssGrpAttr.u8VpssDev = 1;
    pgrp->bBindMode = 0;
    pgrp->astChn[0].enModId = CVI_ID_VI; // src
    pgrp->astChn[0].s32DevId = 0;
    pgrp->astChn[0].s32ChnId = 0;
    pgrp->astChn[1].enModId = CVI_ID_VPSS; // src
    pgrp->astChn[1].s32DevId = 0;
    pgrp->astChn[1].s32ChnId = 0;
    pgrp->abChnEnable[0] = 1; // ch0
    pgrp->aAttachEn[0] = 1;
    pgrp->aAttachPool[0] = 0;
    pgrp->astVpssChnAttr[0] = chn_attr;
    pgrp->abChnEnable[1] = 1; // ch1
    pgrp->aAttachEn[1] = 1;
    pgrp->aAttachPool[1] = 1;
    pgrp->astVpssChnAttr[1] = chn_attr;
    pgrp->abChnEnable[2] = 1; // ch2
    pgrp->aAttachEn[2] = 1;
    pgrp->aAttachPool[2] = 2;
    pgrp->astVpssChnAttr[2] = chn_attr;

    // venc
    APP_PARAM_VENC_CTX_S* venc = app_ipcam_Venc_Param_Get();
    venc->s32VencChnCnt = 3;
    APP_VENC_CHN_CFG_S* pvchn;
    venc->astVencChnCfg[0] = venc_h265;
    pvchn = &venc->astVencChnCfg[0];
    pvchn->enGopMode = VENC_GOPMODE_NORMALP;
    pvchn->unGopParam = h265_stNormalP;
    pvchn->enRcMode = VENC_RC_MODE_H265CBR;
    pvchn->stRcParam = h265_stRcParam;
    pvchn->VencChn = 0;
    pvchn->VpssGrp = 0;
    pvchn->VpssChn = 0;
    pvchn->astChn[0].enModId = CVI_ID_VPSS;
    pvchn->astChn[0].s32DevId = 0; // src
    pvchn->astChn[0].s32ChnId = 0;
    pvchn->astChn[1].enModId = CVI_ID_VENC;
    pvchn->astChn[1].s32DevId = 0; // dst
    pvchn->astChn[1].s32ChnId = 0;

    venc->astVencChnCfg[1] = venc_h264;
    pvchn = &venc->astVencChnCfg[1];
    pvchn->enGopMode = VENC_GOPMODE_NORMALP;
    pvchn->unGopParam = h264_stNormalP;
    pvchn->enRcMode = VENC_RC_MODE_H264CBR;
    pvchn->stRcParam = h264_stRcParam;
    pvchn->VencChn = 1;
    pvchn->VpssGrp = 0;
    pvchn->VpssChn = 1;
    pvchn->astChn[0].enModId = CVI_ID_VPSS;
    pvchn->astChn[0].s32DevId = 0; // src
    pvchn->astChn[0].s32ChnId = 1;
    pvchn->astChn[1].enModId = CVI_ID_VENC;
    pvchn->astChn[1].s32DevId = 0; // dst
    pvchn->astChn[1].s32ChnId = 1;

    venc->astVencChnCfg[2] = venc_jpeg;
    pvchn = &venc->astVencChnCfg[2];
    pvchn->VencChn = 2;
    pvchn->VpssGrp = 0;
    pvchn->VpssChn = 2;
    pvchn->astChn[0].enModId = CVI_ID_VPSS;
    pvchn->astChn[0].s32DevId = 0; // src
    pvchn->astChn[0].s32ChnId = 2;
    pvchn->astChn[1].enModId = CVI_ID_VENC;
    pvchn->astChn[1].s32DevId = 0; // dst
    pvchn->astChn[1].s32ChnId = 2;

    return CVI_SUCCESS;
}
