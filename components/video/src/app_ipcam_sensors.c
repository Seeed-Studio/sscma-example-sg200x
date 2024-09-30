#include "app_ipcam_paramparse.h"

static const VI_DEV_ATTR_S vi_dev_attr_base = {
    .enIntfMode = VI_MODE_MIPI,
    .enWorkMode = VI_WORK_MODE_1Multiplex,
    .enScanMode = VI_SCAN_PROGRESSIVE,
    .as32AdChnId = { -1, -1, -1, -1 },
    .enDataSeq = VI_DATA_SEQ_YUYV,
    .stSynCfg = {
        /*port_vsync    port_vsync_neg    port_hsync              port_hsync_neg*/
        VI_VSYNC_PULSE,
        VI_VSYNC_NEG_LOW,
        VI_HSYNC_VALID_SINGNAL,
        VI_HSYNC_NEG_HIGH,
        /*port_vsync_valid     port_vsync_valid_neg*/
        VI_VSYNC_VALID_SIGNAL,
        VI_VSYNC_VALID_NEG_HIGH,

        .stTimingBlank = {
            /*hsync_hfb  hsync_act  hsync_hhb*/
            0, 1920, 0,
            /*vsync0_vhb vsync0_act vsync0_hhb*/
            0, 1080, 0,
            /*vsync1_vhb vsync1_act vsync1_hhb*/
            0, 0, 0 },
    },
    .enInputDataType = VI_DATA_TYPE_RGB,
    .stSize = { 1920, 1080 },
    .stWDRAttr = { WDR_MODE_NONE, 1080 },
    .enBayerFormat = BAYER_FORMAT_BG,
};

static const VI_PIPE_ATTR_S vi_pipe_attr_base = {
    .enPipeBypassMode = VI_PIPE_BYPASS_NONE,
    .bYuvSkip = CVI_FALSE,
    .bIspBypass = CVI_FALSE,
    .u32MaxW = 1920,
    .u32MaxH = 1080,
    .enPixFmt = PIXEL_FORMAT_RGB_BAYER_12BPP,
    .enCompressMode = COMPRESS_MODE_NONE,
    .enBitWidth = DATA_BITWIDTH_12,
    .bNrEn = CVI_TRUE,
    .bSharpenEn = CVI_FALSE,
    .stFrameRate = { -1, -1 },
    .bDiscardProPic = CVI_FALSE,
    .bYuvBypassPath = CVI_FALSE,
};

static const VI_CHN_ATTR_S vi_chn_attr_base = {
    .stSize = { 1920, 1080 },
    .enPixelFormat = PIXEL_FORMAT_YUV_PLANAR_420,
    .enDynamicRange = DYNAMIC_RANGE_SDR8,
    .enVideoFormat = VIDEO_FORMAT_LINEAR,
    .enCompressMode = COMPRESS_MODE_NONE,
    .bMirror = CVI_FALSE,
    .bFlip = CVI_FALSE,
    .u32Depth = 0,
    .stFrameRate = { -1, -1 },
};

static const ISP_PUB_ATTR_S isp_pub_attr_base = {
    .stWndRect = { 0, 0, 1920, 1080 },
    .stSnsSize = { 1920, 1080 },
    .f32FrameRate = 25.0f,
    .enBayer = BAYER_BGGR,
    .enWDRMode = WDR_MODE_NONE,
    .u8SnsMode = 0,
};

ISP_SNS_OBJ_S* app_ipcam_SnsObj_Get(SENSOR_TYPE_E enSnsType)
{
    switch (enSnsType) {
#ifdef SNS0_GCORE_GC1054
    case SENSOR_GCORE_GC1054:
        return &stSnsGc1054_Obj;
#endif
#ifdef SNS0_GCORE_GC2053
    case SENSOR_GCORE_GC2053:
        return &stSnsGc2053_Obj;
#endif
#ifdef SNS0_GCORE_GC2053_1L
    case SENSOR_GCORE_GC2053_1L:
        return &stSnsGc2053_1l_Obj;
#endif
#ifdef SNS1_GCORE_GC2053_SLAVE
    case SENSOR_GCORE_GC2053_SLAVE:
        return &stSnsGc2053_Slave_Obj;
#endif
#ifdef SNS0_GCORE_GC2093
    case SENSOR_GCORE_GC2093:
        return &stSnsGc2093_Obj;
#endif
#ifdef SNS1_GCORE_GC2093_SLAVE
    case SENSOR_GCORE_GC2093_SLAVE:
        return &stSnsGc2093_Slave_Obj;
#endif
#ifdef SNS0_GCORE_GC4023
    case SENSOR_GCORE_GC4023:
        return &stSnsGc4023_Obj;
#endif
#ifdef SNS0_GCORE_GC4653
    case SENSOR_GCORE_GC4653:
        return &stSnsGc4653_Obj;
#endif
#ifdef SNS1_GCORE_GC4653_SLAVE
    case SENSOR_GCORE_GC4653_SLAVE:
        return &stSnsGc4653_Slave_Obj;
#endif
#ifdef SNS0_NEXTCHIP_N5
    case SENSOR_NEXTCHIP_N5:
        return &stSnsN5_Obj;
#endif
#ifdef SNS0_NEXTCHIP_N6
    case SENSOR_NEXTCHIP_N6:
        return &stSnsN6_Obj;
#endif
#ifdef SNS0_OV_OV5647
    case SENSOR_OV_OV5647:
        return &stSnsOv5647_Obj;
#endif
#ifdef SNS0_OV_OS08A20
    case SENSOR_OV_OS08A20:
        return &stSnsOs08a20_Obj;
#endif
#ifdef SNS1_OV_OS08A20_SLAVE
    case SENSOR_OV_OS08A20_SLAVE:
        return &stSnsOs08a20_Slave_Obj;
#endif
#ifdef PICO_384
    case SENSOR_PICO_384:
        return &stSnsPICO384_Obj;
#endif
#ifdef SNS0_PICO_640
    case SENSOR_PICO_640:
        return &stSnsPICO640_Obj;
#endif
#ifdef SNS1_PIXELPLUS_PR2020
    case SENSOR_PIXELPLUS_PR2020:
        return &stSnsPR2020_Obj;
#endif
#ifdef SNS0_PIXELPLUS_PR2100
    case SENSOR_PIXELPLUS_PR2100:
        return &stSnsPR2100_Obj;
#endif
#ifdef SNS0_SMS_SC1346_1L
    case SENSOR_SMS_SC1346_1L:
    case SENSOR_SMS_SC1346_1L_60:
        return &stSnsSC1346_1L_Obj;
#endif
#ifdef SNS0_SMS_SC200AI
    case SENSOR_SMS_SC200AI:
        return &stSnsSC200AI_Obj;
#endif
#ifdef SNS0_SMS_SC2331_1L
    case SENSOR_SMS_SC2331_1L:
        return &stSnsSC2331_1L_Obj;
#endif
#ifdef SNS0_SMS_SC2335
    case SENSOR_SMS_SC2335:
        return &stSnsSC2335_Obj;
#endif
#ifdef SNS0_SMS_SC2336
    case SENSOR_SMS_SC2336:
        return &stSnsSC2336_Obj;
#endif
#ifdef SNS0_SMS_SC2336P
    case SENSOR_SMS_SC2336P:
        return &stSnsSC2336P_Obj;
#endif
#ifdef SNS0_SMS_SC3335
    case SENSOR_SMS_SC3335:
        return &stSnsSC3335_Obj;
#endif
#ifdef SNS1_SMS_SC3335_SLAVE
    case SENSOR_SMS_SC3335_SLAVE:
        return &stSnsSC3335_Slave_Obj;
#endif
#ifdef SNS0_SMS_SC3336
    case SENSOR_SMS_SC3336:
        return &stSnsSC3336_Obj;
#endif
#ifdef SNS0_SMS_SC401AI
    case SENSOR_SMS_SC401AI:
        return &stSnsSC401AI_Obj;
#endif
#ifdef SNS0_SMS_SC4210
    case SENSOR_SMS_SC4210:
        return &stSnsSC4210_Obj;
#endif
#ifdef SNS0_SMS_SC8238
    case SENSOR_SMS_SC8238:
        return &stSnsSC8238_Obj;
#endif
#ifdef SNS0_SMS_SC530AI_2L
    case SENSOR_SMS_SC530AI_2L:
        return &stSnsSC530AI_2L_Obj;
#endif
#ifdef SNS0_SMS_SC531AI_2L
    case SENSOR_SMS_SC531AI_2L:
        return &stSnsSC531AI_2L_Obj;
#endif
#ifdef SNS0_SMS_SC5336_2L
    case SENSOR_SMS_SC5336_2L:
        return &stSnsSC5336_2L_Obj;
#endif
#ifdef SNS0_SMS_SC4336P
    case SENSOR_SMS_SC4336P:
        return &stSnsSC4336P_Obj;
#endif
#ifdef SNS0_SOI_F23
    case SENSOR_SOI_F23:
        return &stSnsF23_Obj;
#endif
#ifdef SNS0_SOI_F35
    case SENSOR_SOI_F35:
        return &stSnsF35_Obj;
#endif
#ifdef SNS1_SOI_F35_SLAVE
    case SENSOR_SOI_F35_SLAVE:
        return &stSnsF35_Slave_Obj;
#endif
#ifdef SNS0_SOI_H65
    case SENSOR_SOI_H65:
        return &stSnsH65_Obj;
#endif
#ifdef SNS0_SOI_K06
    case SENSOR_SOI_K06:
        return &stSnsK06_Obj;
#endif
#ifdef SNS0_SOI_Q03P
    case SENSOR_SOI_Q03P:
        return &stSnsQ03P_Obj;
#endif
#ifdef SNS0_SONY_IMX290_2L
    case SENSOR_SONY_IMX290_2L:
        return &stSnsImx290_2l_Obj;
#endif
#ifdef SNS0_SONY_IMX307
    case SENSOR_SONY_IMX307:
        return &stSnsImx307_Obj;
#endif
#ifdef SNS0_SONY_IMX307_2L
    case SENSOR_SONY_IMX307_2L:
        return &stSnsImx307_2l_Obj;
#endif
#ifdef SNS1_SONY_IMX307_SLAVE
    case SENSOR_SONY_IMX307_SLAVE:
        return &stSnsImx307_Slave_Obj;
#endif
#ifdef SNS0_SONY_IMX307_SUBLVDS
    case SENSOR_SONY_IMX307_SUBLVDS:
        return &stSnsImx307_Sublvds_Obj;
#endif
#ifdef SNS0_SONY_IMX327
    case SENSOR_SONY_IMX327:
        return &stSnsImx327_Obj;
#endif
#ifdef SNS0_SONY_IMX327_2L
    case SENSOR_SONY_IMX327_2L:
        return &stSnsImx327_2l_Obj;
#endif
#ifdef SNS1_SONY_IMX327_SLAVE
    case SENSOR_SONY_IMX327_SLAVE:
        return &stSnsImx327_Slave_Obj;
#endif
#ifdef SNS0_SONY_IMX327_SUBLVDS
    case SENSOR_SONY_IMX327_SUBLVDS:
        return &stSnsImx327_Sublvds_Obj;
#endif
#ifdef SNS0_SONY_IMX334
    case SENSOR_SONY_IMX334:
        return &stSnsImx334_Obj;
#endif
#ifdef SNS0_SONY_IMX335
    case SENSOR_SONY_IMX335:
        return &stSnsImx335_Obj;
#endif
#ifdef SNS0_SONY_IMX347
    case SENSOR_SONY_IMX347:
        return &stSnsImx347_Obj;
#endif
#ifdef SNS0_SONY_IMX385
    case SENSOR_SONY_IMX385:
        return &stSnsImx385_Obj;
#endif
#ifdef SNS0_VIVO_MCS369
    case SENSOR_VIVO_MCS369:
        return &stSnsMCS369_Obj;
#endif
#ifdef SNS0_VIVO_MCS369Q
    case SENSOR_VIVO_MCS369Q:
        return &stSnsMCS369Q_Obj;
#endif
#ifdef SNS0_VIVO_MM308M2
    case SENSOR_VIVO_MM308M2:
        return &stSnsMM308M2_Obj;
#endif
#ifdef SENSOR_ID_MIS2008
    case SENSOR_IMGDS_MIS2008:
        return &stSnsMIS2008_Obj;
#endif
#ifdef SENSOR_ID_MIS2008_1L
    case SENSOR_IMGDS_MIS2008_1L:
        return &stSnsMIS2008_1L_Obj;
#endif
    default:
        return CVI_NULL;
    }
}

CVI_S32 app_ipcam_Vi_DevAttr_Get(SENSOR_TYPE_E enSnsType, VI_DEV_ATTR_S* pstViDevAttr)
{
    CVI_S32 s32Ret = CVI_SUCCESS;

    memcpy(pstViDevAttr, &vi_dev_attr_base, sizeof(VI_DEV_ATTR_S));

    switch (enSnsType) {
    case SENSOR_SMS_SC530AI_2L:
        pstViDevAttr->stSynCfg.stTimingBlank.u32HsyncAct = 2880;
        pstViDevAttr->stSynCfg.stTimingBlank.u32VsyncVact = 1620;
        pstViDevAttr->stSize.u32Width = 2880;
        pstViDevAttr->stSize.u32Height = 1620;
        pstViDevAttr->stWDRAttr.u32CacheLine = 1620;
        break;
    }

    switch (enSnsType) {
    case SENSOR_GCORE_GC1054:
    case SENSOR_GCORE_GC2053:
    case SENSOR_GCORE_GC2053_1L:
    case SENSOR_GCORE_GC2053_SLAVE:
    case SENSOR_GCORE_GC2093:
    case SENSOR_GCORE_GC4023:
    case SENSOR_GCORE_GC2093_SLAVE:
        pstViDevAttr->enBayerFormat = BAYER_FORMAT_RG;
        break;
    case SENSOR_GCORE_GC4653:
    case SENSOR_GCORE_GC4653_SLAVE:
        pstViDevAttr->enBayerFormat = BAYER_FORMAT_GR;
        break;
    case SENSOR_NEXTCHIP_N5:
        pstViDevAttr->enIntfMode = VI_MODE_BT656;
        pstViDevAttr->enDataSeq = VI_DATA_SEQ_UYVY;
        pstViDevAttr->enInputDataType = VI_DATA_TYPE_YUV;
        break;
    case SENSOR_NEXTCHIP_N6:
        pstViDevAttr->enDataSeq = VI_DATA_SEQ_UYVY;
        pstViDevAttr->enInputDataType = VI_DATA_TYPE_YUV;
        break;
    case SENSOR_OV_OS08A20:
    case SENSOR_OV_OS08A20_SLAVE:
    case SENSOR_OV_OV5647:
        pstViDevAttr->enBayerFormat = BAYER_FORMAT_BG;
        break;
    case SENSOR_PICO_384:
    case SENSOR_PICO_640:
        break;
    case SENSOR_PIXELPLUS_PR2020:
        pstViDevAttr->enIntfMode = VI_MODE_BT656;
        pstViDevAttr->enDataSeq = VI_DATA_SEQ_UYVY;
        pstViDevAttr->enInputDataType = VI_DATA_TYPE_YUV;
        break;
    case SENSOR_PIXELPLUS_PR2100:
        pstViDevAttr->enIntfMode = VI_MODE_MIPI_YUV422;
        pstViDevAttr->enDataSeq = VI_DATA_SEQ_UYVY;
        pstViDevAttr->enInputDataType = VI_DATA_TYPE_YUV;
        break;
    case SENSOR_SMS_SC1346_1L:
    case SENSOR_SMS_SC1346_1L_60:
    case SENSOR_SMS_SC200AI:
    case SENSOR_SMS_SC2331_1L:
    case SENSOR_SMS_SC2335:
    case SENSOR_SMS_SC2336:
    case SENSOR_SMS_SC2336P:
    case SENSOR_SMS_SC3335:
    case SENSOR_SMS_SC3335_SLAVE:
    case SENSOR_SMS_SC3336:
    case SENSOR_SMS_SC401AI:
    case SENSOR_SMS_SC4210:
    case SENSOR_SMS_SC8238:
    case SENSOR_SMS_SC530AI_2L:
    case SENSOR_SMS_SC531AI_2L:
    case SENSOR_SMS_SC5336_2L:
    case SENSOR_SMS_SC4336P:
        pstViDevAttr->enBayerFormat = BAYER_FORMAT_BG;
        break;
    case SENSOR_SOI_F23:
    case SENSOR_SOI_F35:
    case SENSOR_SOI_F35_SLAVE:
    case SENSOR_SOI_H65:
        pstViDevAttr->enBayerFormat = BAYER_FORMAT_BG;
        break;
    case SENSOR_SOI_K06:
        pstViDevAttr->enBayerFormat = BAYER_FORMAT_GB;
        break;
    case SENSOR_SOI_Q03P:
        pstViDevAttr->enBayerFormat = BAYER_FORMAT_BG;
        break;
    case SENSOR_SONY_IMX290_2L:
        pstViDevAttr->enBayerFormat = BAYER_FORMAT_BG;
        break;
    case SENSOR_SONY_IMX307:
    case SENSOR_SONY_IMX307_2L:
    case SENSOR_SONY_IMX307_SLAVE:
        pstViDevAttr->enBayerFormat = BAYER_FORMAT_RG;
        break;
    case SENSOR_SONY_IMX307_SUBLVDS:
        pstViDevAttr->enIntfMode = VI_MODE_LVDS;
        pstViDevAttr->enBayerFormat = BAYER_FORMAT_RG;
        break;
    case SENSOR_SONY_IMX327:
    case SENSOR_SONY_IMX327_2L:
    case SENSOR_SONY_IMX327_SLAVE:
        pstViDevAttr->enBayerFormat = BAYER_FORMAT_RG;
        break;
    case SENSOR_SONY_IMX327_SUBLVDS:
        pstViDevAttr->enIntfMode = VI_MODE_LVDS;
        pstViDevAttr->enBayerFormat = BAYER_FORMAT_RG;
        break;
    case SENSOR_SONY_IMX334:
    case SENSOR_SONY_IMX335:
    case SENSOR_SONY_IMX347:
    case SENSOR_SONY_IMX385:
        pstViDevAttr->enBayerFormat = BAYER_FORMAT_RG;
        break;
    case SENSOR_VIVO_MCS369:
    case SENSOR_VIVO_MCS369Q:
    case SENSOR_VIVO_MM308M2:
        pstViDevAttr->enIntfMode = VI_MODE_BT1120_STANDARD;
        pstViDevAttr->enInputDataType = VI_DATA_TYPE_YUV;
        break;
    case SENSOR_IMGDS_MIS2008:
    case SENSOR_IMGDS_MIS2008_1L:
        pstViDevAttr->enBayerFormat = BAYER_FORMAT_RG;
        break;
    default:
        s32Ret = CVI_FAILURE;
        break;
    }
    return s32Ret;
}

CVI_S32 app_ipcam_Vi_PipeAttr_Get(SENSOR_TYPE_E enSnsType, VI_PIPE_ATTR_S* pstViPipeAttr)
{
    CVI_S32 s32Ret = CVI_SUCCESS;

    memcpy(pstViPipeAttr, &vi_pipe_attr_base, sizeof(VI_PIPE_ATTR_S));

    switch (enSnsType) {
    case SENSOR_SMS_SC530AI_2L:
        pstViPipeAttr->u32MaxW = 2880;
        pstViPipeAttr->u32MaxH = 1620;
        break;
    }

    switch (enSnsType) {
    case SENSOR_GCORE_GC1054:
    case SENSOR_GCORE_GC2053:
    case SENSOR_GCORE_GC2053_1L:
    case SENSOR_OV_OV5647:
    case SENSOR_GCORE_GC2053_SLAVE:
    case SENSOR_GCORE_GC2093:
    case SENSOR_GCORE_GC2093_SLAVE:
    case SENSOR_GCORE_GC4023:
    case SENSOR_GCORE_GC4653:
    case SENSOR_GCORE_GC4653_SLAVE:
        break;
    case SENSOR_NEXTCHIP_N5:
    case SENSOR_NEXTCHIP_N6:
        pstViPipeAttr->bYuvBypassPath = CVI_TRUE;
        break;
    case SENSOR_OV_OS08A20:
    case SENSOR_OV_OS08A20_SLAVE:
        break;
    case SENSOR_PICO_384:
    case SENSOR_PICO_640:
    case SENSOR_PIXELPLUS_PR2020:
    case SENSOR_PIXELPLUS_PR2100:
        pstViPipeAttr->bYuvBypassPath = CVI_TRUE;
        break;
    case SENSOR_SMS_SC1346_1L:
    case SENSOR_SMS_SC1346_1L_60:
    case SENSOR_SMS_SC200AI:
    case SENSOR_SMS_SC2331_1L:
    case SENSOR_SMS_SC2335:
    case SENSOR_SMS_SC2336:
    case SENSOR_SMS_SC2336P:
    case SENSOR_SMS_SC3335:
    case SENSOR_SMS_SC3335_SLAVE:
    case SENSOR_SMS_SC3336:
    case SENSOR_SMS_SC401AI:
    case SENSOR_SMS_SC4210:
    case SENSOR_SMS_SC8238:
    case SENSOR_SMS_SC530AI_2L:
    case SENSOR_SMS_SC531AI_2L:
    case SENSOR_SMS_SC5336_2L:
    case SENSOR_SMS_SC4336P:
    case SENSOR_SOI_F23:
    case SENSOR_SOI_F35:
    case SENSOR_SOI_F35_SLAVE:
    case SENSOR_SOI_H65:
    case SENSOR_SOI_K06:
    case SENSOR_SOI_Q03P:
    case SENSOR_SONY_IMX290_2L:
    case SENSOR_SONY_IMX307:
    case SENSOR_SONY_IMX307_2L:
    case SENSOR_SONY_IMX307_SLAVE:
    case SENSOR_SONY_IMX307_SUBLVDS:
    case SENSOR_SONY_IMX327:
    case SENSOR_SONY_IMX327_2L:
    case SENSOR_SONY_IMX327_SLAVE:
    case SENSOR_SONY_IMX327_SUBLVDS:
    case SENSOR_SONY_IMX334:
    case SENSOR_SONY_IMX335:
    case SENSOR_SONY_IMX347:
    case SENSOR_SONY_IMX385:
    case SENSOR_IMGDS_MIS2008:
    case SENSOR_IMGDS_MIS2008_1L:
        break;
    case SENSOR_VIVO_MCS369:
    case SENSOR_VIVO_MCS369Q:
    case SENSOR_VIVO_MM308M2:
        pstViPipeAttr->bYuvBypassPath = CVI_TRUE;
        break;
    default:
        s32Ret = CVI_FAILURE;
        break;
    }
    return s32Ret;
}

CVI_S32 app_ipcam_Vi_ChnAttr_Get(SENSOR_TYPE_E enSnsType, VI_CHN_ATTR_S* pstViChnAttr)
{
    CVI_S32 s32Ret = CVI_SUCCESS;

    memcpy(pstViChnAttr, &vi_chn_attr_base, sizeof(VI_CHN_ATTR_S));

    switch (enSnsType) {
    case SENSOR_SMS_SC530AI_2L:
        pstViChnAttr->stSize.u32Width = 2880;
        pstViChnAttr->stSize.u32Height = 1620;
        break;
    }

    switch (enSnsType) {
    case SENSOR_GCORE_GC1054:
    case SENSOR_OV_OV5647:
    case SENSOR_GCORE_GC2053:
    case SENSOR_GCORE_GC2053_1L:
    case SENSOR_GCORE_GC2053_SLAVE:
    case SENSOR_GCORE_GC2093:
    case SENSOR_GCORE_GC2093_SLAVE:
    case SENSOR_GCORE_GC4023:
    case SENSOR_GCORE_GC4653:
    case SENSOR_GCORE_GC4653_SLAVE:
        break;
    case SENSOR_NEXTCHIP_N5:
    case SENSOR_NEXTCHIP_N6:
        pstViChnAttr->enPixelFormat = PIXEL_FORMAT_YUV_PLANAR_422;
        break;
    case SENSOR_OV_OS08A20:
    case SENSOR_OV_OS08A20_SLAVE:
        break;
    case SENSOR_PICO_384:
    case SENSOR_PICO_640:
    case SENSOR_PIXELPLUS_PR2020:
    case SENSOR_PIXELPLUS_PR2100:
        pstViChnAttr->enPixelFormat = PIXEL_FORMAT_YUV_PLANAR_422;
        break;
    case SENSOR_SMS_SC1346_1L:
    case SENSOR_SMS_SC1346_1L_60:
    case SENSOR_SMS_SC200AI:
    case SENSOR_SMS_SC2331_1L:
    case SENSOR_SMS_SC2335:
    case SENSOR_SMS_SC2336:
    case SENSOR_SMS_SC2336P:
    case SENSOR_SMS_SC3335:
    case SENSOR_SMS_SC3335_SLAVE:
    case SENSOR_SMS_SC3336:
    case SENSOR_SMS_SC401AI:
    case SENSOR_SMS_SC4210:
    case SENSOR_SMS_SC8238:
    case SENSOR_SMS_SC530AI_2L:
    case SENSOR_SMS_SC531AI_2L:
    case SENSOR_SMS_SC5336_2L:
    case SENSOR_SMS_SC4336P:
    case SENSOR_SOI_F23:
    case SENSOR_SOI_F35:
    case SENSOR_SOI_F35_SLAVE:
    case SENSOR_SOI_H65:
    case SENSOR_SOI_K06:
    case SENSOR_SOI_Q03P:
    case SENSOR_SONY_IMX290_2L:
    case SENSOR_SONY_IMX307:
    case SENSOR_SONY_IMX307_2L:
    case SENSOR_SONY_IMX307_SLAVE:
    case SENSOR_SONY_IMX307_SUBLVDS:
    case SENSOR_SONY_IMX327:
    case SENSOR_SONY_IMX327_2L:
    case SENSOR_SONY_IMX327_SLAVE:
    case SENSOR_SONY_IMX327_SUBLVDS:
    case SENSOR_SONY_IMX334:
    case SENSOR_SONY_IMX335:
    case SENSOR_SONY_IMX347:
    case SENSOR_SONY_IMX385:
    case SENSOR_IMGDS_MIS2008:
    case SENSOR_IMGDS_MIS2008_1L:
        break;
    case SENSOR_VIVO_MCS369:
    case SENSOR_VIVO_MCS369Q:
    case SENSOR_VIVO_MM308M2:
        pstViChnAttr->enPixelFormat = PIXEL_FORMAT_YUV_PLANAR_422;
        break;
    default:
        s32Ret = CVI_FAILURE;
        break;
    }
    return s32Ret;
}

CVI_S32 app_ipcam_Isp_InitAttr_Get(SENSOR_TYPE_E enSnsType, WDR_MODE_E enWDRMode, ISP_INIT_ATTR_S* pstIspInitAttr)
{
    CVI_S32 s32Ret = CVI_SUCCESS;

    memset(pstIspInitAttr, 0, sizeof(ISP_INIT_ATTR_S));

    switch (enSnsType) {
    case SENSOR_GCORE_GC1054:
    case SENSOR_GCORE_GC2053:
    case SENSOR_GCORE_GC2053_1L:
    case SENSOR_OV_OV5647:
    case SENSOR_GCORE_GC2053_SLAVE:
    case SENSOR_GCORE_GC2093:
    case SENSOR_GCORE_GC2093_SLAVE:
    case SENSOR_GCORE_GC4023:
    case SENSOR_GCORE_GC4653:
    case SENSOR_GCORE_GC4653_SLAVE:
    case SENSOR_NEXTCHIP_N5:
    case SENSOR_NEXTCHIP_N6:
        break;
    case SENSOR_OV_OS08A20:
    case SENSOR_OV_OS08A20_SLAVE:
        if (enWDRMode == WDR_MODE_2To1_LINE) {
            pstIspInitAttr->enL2SMode = SNS_L2S_MODE_FIX;
        }
        break;
    case SENSOR_PICO_384:
    case SENSOR_PICO_640:
    case SENSOR_PIXELPLUS_PR2020:
    case SENSOR_PIXELPLUS_PR2100:
    case SENSOR_SMS_SC1346_1L:
    case SENSOR_SMS_SC1346_1L_60:
    case SENSOR_SMS_SC200AI:
    case SENSOR_SMS_SC2331_1L:
    case SENSOR_SMS_SC2335:
    case SENSOR_SMS_SC2336:
    case SENSOR_SMS_SC2336P:
    case SENSOR_SMS_SC3335:
    case SENSOR_SMS_SC3335_SLAVE:
    case SENSOR_SMS_SC3336:
    case SENSOR_SMS_SC401AI:
    case SENSOR_SMS_SC4210:
    case SENSOR_SMS_SC8238:
    case SENSOR_SMS_SC530AI_2L:
    case SENSOR_SMS_SC531AI_2L:
    case SENSOR_SMS_SC5336_2L:
    case SENSOR_SMS_SC4336P:
    case SENSOR_SOI_F23:
    case SENSOR_IMGDS_MIS2008:
    case SENSOR_IMGDS_MIS2008_1L:
        break;
    case SENSOR_SOI_F35:
    case SENSOR_SOI_F35_SLAVE:
        if (enWDRMode == WDR_MODE_2To1_LINE) {
            pstIspInitAttr->enL2SMode = SNS_L2S_MODE_FIX;
        }
        break;
    case SENSOR_SOI_H65:
    case SENSOR_SOI_K06:
    case SENSOR_SOI_Q03P:
    case SENSOR_SONY_IMX290_2L:
    case SENSOR_SONY_IMX307:
    case SENSOR_SONY_IMX307_2L:
    case SENSOR_SONY_IMX307_SLAVE:
    case SENSOR_SONY_IMX307_SUBLVDS:
    case SENSOR_SONY_IMX327:
    case SENSOR_SONY_IMX327_2L:
    case SENSOR_SONY_IMX327_SLAVE:
    case SENSOR_SONY_IMX327_SUBLVDS:
    case SENSOR_SONY_IMX334:
    case SENSOR_SONY_IMX335:
    case SENSOR_SONY_IMX347:
    case SENSOR_SONY_IMX385:
    case SENSOR_VIVO_MCS369:
    case SENSOR_VIVO_MCS369Q:
    case SENSOR_VIVO_MM308M2:
        break;
    default:
        s32Ret = CVI_FAILURE;
        break;
    }
    return s32Ret;
}

CVI_S32 app_ipcam_Isp_PubAttr_Get(SENSOR_TYPE_E enSnsType, ISP_PUB_ATTR_S* pstIspPubAttr)
{
    CVI_S32 s32Ret = CVI_SUCCESS;

    memcpy(pstIspPubAttr, &isp_pub_attr_base, sizeof(ISP_PUB_ATTR_S));
    switch (enSnsType) {
    case SENSOR_SMS_SC530AI_2L:
        pstIspPubAttr->stWndRect.u32Width = 2880;
        pstIspPubAttr->stWndRect.u32Height = 1620;
        pstIspPubAttr->stSnsSize.u32Width = 2880;
        pstIspPubAttr->stSnsSize.u32Height = 1620;
        break;
    }

    // FPS
    switch (enSnsType) {
    case SENSOR_SMS_SC1346_1L_60:
        pstIspPubAttr->f32FrameRate = 60;
        break;
    default:
        pstIspPubAttr->f32FrameRate = 25;
        break;
    }
    switch (enSnsType) {
    case SENSOR_GCORE_GC1054:
    case SENSOR_GCORE_GC2053:
    case SENSOR_GCORE_GC2053_1L:
    case SENSOR_GCORE_GC2053_SLAVE:
    case SENSOR_GCORE_GC2093:
    case SENSOR_GCORE_GC4023:
    case SENSOR_GCORE_GC2093_SLAVE:
        pstIspPubAttr->enBayer = BAYER_RGGB;
        break;
    case SENSOR_GCORE_GC4653:
    case SENSOR_GCORE_GC4653_SLAVE:
        pstIspPubAttr->enBayer = BAYER_GRBG;
        break;
    case SENSOR_NEXTCHIP_N5:
    case SENSOR_NEXTCHIP_N6:
        pstIspPubAttr->enBayer = BAYER_BGGR;
        break;
    case SENSOR_OV_OS08A20:
    case SENSOR_OV_OS08A20_SLAVE:
    case SENSOR_OV_OV5647:
        pstIspPubAttr->enBayer = BAYER_BGGR;
        break;
    case SENSOR_PICO_384:
    case SENSOR_PICO_640:
    case SENSOR_PIXELPLUS_PR2020:
    case SENSOR_PIXELPLUS_PR2100:
        pstIspPubAttr->enBayer = BAYER_BGGR;
        break;
    case SENSOR_SMS_SC1346_1L:
    case SENSOR_SMS_SC1346_1L_60:
    case SENSOR_SMS_SC200AI:
    case SENSOR_SMS_SC2331_1L:
    case SENSOR_SMS_SC2335:
    case SENSOR_SMS_SC2336:
    case SENSOR_SMS_SC2336P:
    case SENSOR_SMS_SC3335:
    case SENSOR_SMS_SC3335_SLAVE:
    case SENSOR_SMS_SC3336:
    case SENSOR_SMS_SC401AI:
    case SENSOR_SMS_SC4210:
    case SENSOR_SMS_SC8238:
    case SENSOR_SMS_SC530AI_2L:
    case SENSOR_SMS_SC531AI_2L:
    case SENSOR_SMS_SC5336_2L:
    case SENSOR_SMS_SC4336P:
        pstIspPubAttr->enBayer = BAYER_BGGR;
        break;
    case SENSOR_SOI_F23:
    case SENSOR_SOI_F35:
    case SENSOR_SOI_F35_SLAVE:
    case SENSOR_SOI_H65:
        pstIspPubAttr->enBayer = BAYER_BGGR;
        break;
    case SENSOR_SOI_K06:
        pstIspPubAttr->enBayer = BAYER_GBRG;
        break;
    case SENSOR_SOI_Q03P:
        pstIspPubAttr->enBayer = BAYER_BGGR;
        break;
    case SENSOR_SONY_IMX290_2L:
        pstIspPubAttr->enBayer = BAYER_BGGR;
        break;
    case SENSOR_SONY_IMX307:
    case SENSOR_SONY_IMX307_2L:
    case SENSOR_SONY_IMX307_SLAVE:
    case SENSOR_SONY_IMX307_SUBLVDS:
    case SENSOR_SONY_IMX327:
    case SENSOR_SONY_IMX327_2L:
    case SENSOR_SONY_IMX327_SLAVE:
    case SENSOR_SONY_IMX327_SUBLVDS:
    case SENSOR_SONY_IMX334:
    case SENSOR_SONY_IMX335:
    case SENSOR_SONY_IMX347:
    case SENSOR_SONY_IMX385:
        pstIspPubAttr->enBayer = BAYER_RGGB;
        break;
    case SENSOR_VIVO_MCS369:
    case SENSOR_VIVO_MCS369Q:
    case SENSOR_VIVO_MM308M2:
        pstIspPubAttr->enBayer = BAYER_BGGR;
        break;
    case SENSOR_IMGDS_MIS2008:
    case SENSOR_IMGDS_MIS2008_1L:
        pstIspPubAttr->enBayer = BAYER_RGGB;
        break;
    default:
        s32Ret = CVI_FAILURE;
        break;
    }
    return s32Ret;
}
