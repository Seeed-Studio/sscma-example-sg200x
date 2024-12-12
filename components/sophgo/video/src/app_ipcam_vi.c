#include <linux/cvi_comm_sys.h>
#include <cvi_awb.h>

#include "app_ipcam_paramparse.h"

/**************************************************************************
 *                              M A C R O S                               *
 **************************************************************************/

/**************************************************************************
 *                           C O N S T A N T S                            *
 **************************************************************************/

/**************************************************************************
 *                          D A T A    T Y P E S                          *
 **************************************************************************/

/**************************************************************************
 *                         G L O B A L    D A T A                         *
 **************************************************************************/

static APP_PARAM_VI_CTX_S g_stViCtx, *g_pstViCtx = &g_stViCtx;
// static APP_PARAM_VI_PM_DATA_S ViPmData[VI_MAX_DEV_NUM] = { 0 };

/**************************************************************************
 *                 E X T E R N A L    R E F E R E N C E S                 *
 **************************************************************************/

/**************************************************************************
 *               F U N C T I O N    D E C L A R A T I O N S               *
 **************************************************************************/

APP_PARAM_VI_CTX_S *app_ipcam_Vi_Param_Get(void)
{
    return g_pstViCtx;
}

static int app_ipcam_Vi_Sensor_Start(void)
{
    CVI_S32 s32Ret = CVI_SUCCESS;

    CVI_S32 s32SnsId;
    VI_PIPE ViPipe;

    ISP_SNS_OBJ_S *pfnSnsObj = CVI_NULL;
    RX_INIT_ATTR_S rx_init_attr;
    ISP_INIT_ATTR_S isp_init_attr;
    ISP_SNS_COMMBUS_U sns_bus_info;
    ALG_LIB_S ae_lib;
    ALG_LIB_S awb_lib;
    ISP_SENSOR_EXP_FUNC_S isp_sensor_exp_func;
    ISP_PUB_ATTR_S stPubAttr;
    ISP_CMOS_SENSOR_IMAGE_MODE_S isp_cmos_sensor_image_mode;

    for (CVI_U32 i = 0; i < g_pstViCtx->u32WorkSnsCnt; i++) {
        APP_PARAM_SNS_CFG_T *pstSnsCfg = &g_pstViCtx->astSensorCfg[i];
        APP_PARAM_CHN_CFG_T *pstChnCfg = &g_pstViCtx->astChnInfo[i];
        s32SnsId = pstSnsCfg->s32SnsId;
        ViPipe   = pstChnCfg->s32ChnId;

        if (s32SnsId >= VI_MAX_DEV_NUM) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "invalid sensor id: %d\n", s32SnsId);
            return CVI_FAILURE;
        }

        APP_PROF_LOG_PRINT(LEVEL_INFO, "enSnsType enum: %d\n", pstSnsCfg->enSnsType);
        pfnSnsObj = app_ipcam_SnsObj_Get(pstSnsCfg->enSnsType);
        if (pfnSnsObj == CVI_NULL) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR,"sensor obj(%d) is null\n", ViPipe);
            return CVI_FAILURE;
        }

        memset(&rx_init_attr, 0, sizeof(RX_INIT_ATTR_S));
        rx_init_attr.MipiDev       = pstSnsCfg->MipiDev;
        if (pstSnsCfg->bMclkEn) {
            rx_init_attr.stMclkAttr.bMclkEn = CVI_TRUE;
            rx_init_attr.stMclkAttr.u8Mclk  = pstSnsCfg->u8Mclk;
        }

        for (CVI_U32 i = 0; i < (CVI_U32)sizeof(rx_init_attr.as16LaneId)/sizeof(CVI_S16); i++) {
            rx_init_attr.as16LaneId[i] = pstSnsCfg->as16LaneId[i];
        }
        for (CVI_U32 i = 0; i < (CVI_U32)sizeof(rx_init_attr.as8PNSwap)/sizeof(CVI_S8); i++) {
            rx_init_attr.as8PNSwap[i] = pstSnsCfg->as8PNSwap[i];
        }

        if (pfnSnsObj->pfnPatchRxAttr) {
            s32Ret = pfnSnsObj->pfnPatchRxAttr(&rx_init_attr);
            APP_IPCAM_CHECK_RET(s32Ret, "pfnPatchRxAttr(%d) failed!\n", ViPipe);
        }

        s32Ret = app_ipcam_Isp_InitAttr_Get(pstSnsCfg->enSnsType, pstChnCfg->enWDRMode, &isp_init_attr);
        APP_IPCAM_CHECK_RET(s32Ret, "app_ipcam_Isp_InitAttr_Get(%d) failed!\n", ViPipe);

        isp_init_attr.u16UseHwSync = pstSnsCfg->bHwSync;
        if (pfnSnsObj->pfnSetInit) {
            s32Ret = pfnSnsObj->pfnSetInit(ViPipe, &isp_init_attr);
            APP_IPCAM_CHECK_RET(s32Ret, "pfnSetInit(%d) failed!\n", ViPipe);
        }

        memset(&sns_bus_info, 0, sizeof(ISP_SNS_COMMBUS_U));
        sns_bus_info.s8I2cDev = (pstSnsCfg->s32BusId >= 0) ? (CVI_S8)pstSnsCfg->s32BusId : 0x3;
        if (pfnSnsObj->pfnSetBusInfo) {
            s32Ret = pfnSnsObj->pfnSetBusInfo(ViPipe, sns_bus_info);
            APP_IPCAM_CHECK_RET(s32Ret, "pfnSetBusInfo(%d) failed!\n", ViPipe);
        }

        if (pfnSnsObj->pfnPatchI2cAddr) {
            pfnSnsObj->pfnPatchI2cAddr(pstSnsCfg->s32I2cAddr);
        }

        awb_lib.s32Id = ViPipe;
        ae_lib.s32Id = ViPipe;
        strcpy(ae_lib.acLibName, CVI_AE_LIB_NAME);//, sizeof(CVI_AE_LIB_NAME));
        strcpy(awb_lib.acLibName, CVI_AWB_LIB_NAME);//, sizeof(CVI_AWB_LIB_NAME));
        if (pfnSnsObj->pfnRegisterCallback) {
            s32Ret = pfnSnsObj->pfnRegisterCallback(ViPipe, &ae_lib, &awb_lib);
            APP_IPCAM_CHECK_RET(s32Ret, "pfnRegisterCallback(%d) failed!\n", ViPipe);
        }

        memset(&isp_cmos_sensor_image_mode, 0, sizeof(ISP_CMOS_SENSOR_IMAGE_MODE_S));
        if(app_ipcam_Isp_PubAttr_Get(pstSnsCfg->enSnsType, &stPubAttr) != CVI_SUCCESS)
        {
            APP_PROF_LOG_PRINT(LEVEL_INFO, "Can't get sns attr\n");
            return CVI_FALSE;
        }
        isp_cmos_sensor_image_mode.u16Width  = pstChnCfg->u32Width;
        isp_cmos_sensor_image_mode.u16Height = pstChnCfg->u32Height;
        isp_cmos_sensor_image_mode.f32Fps    = stPubAttr.f32FrameRate;
        APP_PROF_LOG_PRINT(LEVEL_INFO, "sensor %d, Width %d, Height %d, FPS %f, wdrMode %d, pfnSnsObj %p\n",
                s32SnsId,
                isp_cmos_sensor_image_mode.u16Width, isp_cmos_sensor_image_mode.u16Height,
                isp_cmos_sensor_image_mode.f32Fps, pstChnCfg->enWDRMode,
                pfnSnsObj);

        if (pfnSnsObj->pfnExpSensorCb) {
            s32Ret = pfnSnsObj->pfnExpSensorCb(&isp_sensor_exp_func);
            APP_IPCAM_CHECK_RET(s32Ret, "pfnExpSensorCb(%d) failed!\n", ViPipe);

            isp_sensor_exp_func.pfn_cmos_sensor_global_init(ViPipe);

            s32Ret = isp_sensor_exp_func.pfn_cmos_set_image_mode(ViPipe, &isp_cmos_sensor_image_mode);
            APP_IPCAM_CHECK_RET(s32Ret, "pfn_cmos_set_image_mode(%d) failed!\n", ViPipe);

            s32Ret = isp_sensor_exp_func.pfn_cmos_set_wdr_mode(ViPipe, pstChnCfg->enWDRMode);
            APP_IPCAM_CHECK_RET(s32Ret, "pfn_cmos_set_wdr_mode(%d) failed!\n", ViPipe);
        }
    }

    return CVI_SUCCESS;
}

static int app_ipcam_Vi_Mipi_Start(void)
{
    CVI_S32 s32Ret;
    VI_PIPE ViPipe;
    ISP_SNS_OBJ_S *pfnSnsObj = CVI_NULL;
    SNS_COMBO_DEV_ATTR_S combo_dev_attr;

    for (CVI_U32 i = 0; i < g_pstViCtx->u32WorkSnsCnt; i++) {
        APP_PARAM_SNS_CFG_T *pstSnsCfg = &g_pstViCtx->astSensorCfg[i];
        APP_PARAM_CHN_CFG_T *pstChnCfg = &g_pstViCtx->astChnInfo[i];
        ViPipe    = pstChnCfg->s32ChnId;

        pfnSnsObj = app_ipcam_SnsObj_Get(pstSnsCfg->enSnsType);
        if (pfnSnsObj == CVI_NULL) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR,"sensor obj(%d) is null\n", ViPipe);
            return CVI_FAILURE;
        }

        memset(&combo_dev_attr, 0, sizeof(SNS_COMBO_DEV_ATTR_S));
        if (pfnSnsObj->pfnGetRxAttr) {
            s32Ret = pfnSnsObj->pfnGetRxAttr(ViPipe, &combo_dev_attr);
            APP_IPCAM_CHECK_RET(s32Ret, "pfnGetRxAttr(%d) failed!\n", ViPipe);
            pstSnsCfg->MipiDev = combo_dev_attr.devno;
            ViPipe = pstSnsCfg->MipiDev;
            APP_PROF_LOG_PRINT(LEVEL_INFO, "sensor %d devno %d\n", i, ViPipe);
        }

        s32Ret = CVI_MIPI_SetSensorReset(ViPipe, 1);
        APP_IPCAM_CHECK_RET(s32Ret, "CVI_MIPI_SetSensorReset(%d) failed!\n", ViPipe);

        s32Ret = CVI_MIPI_SetMipiReset(ViPipe, 1);
        APP_IPCAM_CHECK_RET(s32Ret, "CVI_MIPI_SetMipiReset(%d) failed!\n", ViPipe);

        if ((pstSnsCfg->enSnsType == SENSOR_VIVO_MCS369) ||
            (pstSnsCfg->enSnsType == SENSOR_VIVO_MCS369Q)) {
            CVI_MIPI_SetClkEdge(ViPipe, 0);
        }

        s32Ret = CVI_MIPI_SetMipiAttr(ViPipe, (CVI_VOID*)&combo_dev_attr);
        APP_IPCAM_CHECK_RET(s32Ret, "CVI_MIPI_SetMipiAttr(%d) failed!\n", ViPipe);

        s32Ret = CVI_MIPI_SetSensorClock(ViPipe, 1);
        APP_IPCAM_CHECK_RET(s32Ret, "CVI_MIPI_SetSensorClock(%d) failed!\n", ViPipe);

        usleep(20);
        s32Ret = CVI_MIPI_SetSensorReset(ViPipe, 0);
        APP_IPCAM_CHECK_RET(s32Ret, "CVI_MIPI_SetSensorReset(%d) failed!\n", ViPipe);

        if (pfnSnsObj->pfnSnsProbe) {
            s32Ret = pfnSnsObj->pfnSnsProbe(ViPipe);
            APP_IPCAM_CHECK_RET(s32Ret, "pfnSnsProbe(%d) failed!\n", ViPipe);
        }
    }

    return CVI_SUCCESS;
}

static int app_ipcam_Vi_Dev_Start(void)
{
    CVI_S32 s32Ret;

    VI_DEV         ViDev;
    VI_DEV_ATTR_S  stViDevAttr;

    for (CVI_U32 i = 0; i < g_pstViCtx->u32WorkSnsCnt; i++) {
        APP_PARAM_SNS_CFG_T *pstSnsCfg = &g_pstViCtx->astSensorCfg[i];
        APP_PARAM_CHN_CFG_T *pstChnCfg = &g_pstViCtx->astChnInfo[i];
        ViDev = pstChnCfg->s32ChnId;
        app_ipcam_Vi_DevAttr_Get(pstSnsCfg->enSnsType, &stViDevAttr);

        stViDevAttr.stSize.u32Width     = pstChnCfg->u32Width;
        stViDevAttr.stSize.u32Height    = pstChnCfg->u32Height;
        stViDevAttr.stWDRAttr.enWDRMode = pstChnCfg->enWDRMode;
        s32Ret = CVI_VI_SetDevAttr(ViDev, &stViDevAttr);
        APP_IPCAM_CHECK_RET(s32Ret, "CVI_VI_SetDevAttr(%d) failed!\n", ViDev);

        s32Ret = CVI_VI_EnableDev(ViDev);
        APP_IPCAM_CHECK_RET(s32Ret, "CVI_VI_EnableDev(%d) failed!\n", ViDev);
    }

    return CVI_SUCCESS;
}

static int app_ipcam_Vi_Dev_Stop(void)
{
    CVI_S32 s32Ret;
    VI_DEV ViDev;

    for (CVI_U32 i = 0; i < g_pstViCtx->u32WorkSnsCnt; i++) {
        APP_PARAM_CHN_CFG_T *pstChnCfg = &g_pstViCtx->astChnInfo[i];
        ViDev = pstChnCfg->s32ChnId;
        s32Ret  = CVI_VI_DisableDev(ViDev);

        // CVI_VI_UnRegChnFlipMirrorCallBack(0, ViDev);
        // CVI_VI_UnRegPmCallBack(ViDev);
        // memset(&ViPmData[ViDev], 0, sizeof(struct VI_PM_DATA_S));

        if (s32Ret != CVI_SUCCESS) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_VI_DisableDev failed with %#x!\n", s32Ret);
            return s32Ret;
        }
    }

    return CVI_SUCCESS;
}

static int app_ipcam_Vi_Pipe_Start(void)
{
    CVI_S32 s32Ret;

    VI_PIPE        ViPipe;
    VI_PIPE_ATTR_S stViPipeAttr;

    for (CVI_U32 i = 0; i < g_pstViCtx->u32WorkSnsCnt; i++) {
        APP_PARAM_SNS_CFG_T *pstSnsCfg = &g_pstViCtx->astSensorCfg[i];
        APP_PARAM_CHN_CFG_T *pstChnCfg = &g_pstViCtx->astChnInfo[i];
        APP_PARAM_PIPE_CFG_T *psPipeCfg = &g_pstViCtx->astPipeInfo[i];

        s32Ret = app_ipcam_Vi_PipeAttr_Get(pstSnsCfg->enSnsType, &stViPipeAttr);
        APP_IPCAM_CHECK_RET(s32Ret, "app_ipcam_Vi_PipeAttr_Get failed!\n");

        stViPipeAttr.u32MaxW = pstChnCfg->u32Width;
        stViPipeAttr.u32MaxH = pstChnCfg->u32Height;
        stViPipeAttr.enCompressMode = pstChnCfg->enCompressMode;

        for (int j = 0; j < WDR_MAX_PIPE_NUM; j++) {
            if ((psPipeCfg->aPipe[j] >= 0) && (psPipeCfg->aPipe[j] < WDR_MAX_PIPE_NUM)) {
                ViPipe = psPipeCfg->aPipe[j];

                s32Ret = CVI_VI_CreatePipe(ViPipe, &stViPipeAttr);
                APP_IPCAM_CHECK_RET(s32Ret, "CVI_VI_CreatePipe(%d) failed!\n", ViPipe);

                s32Ret = CVI_VI_StartPipe(ViPipe);
                APP_IPCAM_CHECK_RET(s32Ret, "CVI_VI_StartPipe(%d) failed!\n", ViPipe);
            }
        }
    }

    return CVI_SUCCESS;
}

static int app_ipcam_Vi_Pipe_Stop(void)
{
    CVI_S32 s32Ret;
    VI_PIPE ViPipe;

    for (CVI_U32 i = 0; i < g_pstViCtx->u32WorkSnsCnt; i++) {
        APP_PARAM_PIPE_CFG_T *psPipeCfg = &g_pstViCtx->astPipeInfo[i];
        for (int j = 0; j < WDR_MAX_PIPE_NUM; j++) {
            ViPipe = psPipeCfg->aPipe[j];
            s32Ret = CVI_VI_StopPipe(ViPipe);
            if (s32Ret != CVI_SUCCESS) {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_VI_StopPipe failed with %#x!\n", s32Ret);
                return s32Ret;
            }

            s32Ret = CVI_VI_DestroyPipe(ViPipe);
            if (s32Ret != CVI_SUCCESS) {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_VI_DestroyPipe failed with %#x!\n", s32Ret);
                return s32Ret;
            }
        }
    }
    
    return CVI_SUCCESS;
}

static int app_ipcam_Vi_Chn_Start(void)
{
    CVI_S32 s32Ret;

    VI_PIPE        ViPipe;
    VI_CHN         ViChn;
    VI_DEV_ATTR_S  stViDevAttr;
    VI_CHN_ATTR_S  stViChnAttr;
    ISP_SNS_OBJ_S  *pstSnsObj = CVI_NULL;

    for (CVI_U32 i = 0; i < g_pstViCtx->u32WorkSnsCnt; i++) {
        APP_PARAM_SNS_CFG_T *pstSnsCfg = &g_pstViCtx->astSensorCfg[i];
        APP_PARAM_CHN_CFG_T *pstChnCfg = &g_pstViCtx->astChnInfo[i];
        ViPipe = pstChnCfg->s32ChnId;
        ViChn = pstChnCfg->s32ChnId;

        pstSnsObj = app_ipcam_SnsObj_Get(pstSnsCfg->enSnsType);
        if (pstSnsObj == CVI_NULL) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "sensor obj(%d) is null\n", ViPipe);
            return CVI_FAILURE;
        }

        s32Ret = app_ipcam_Vi_DevAttr_Get(pstSnsCfg->enSnsType, &stViDevAttr);
        APP_IPCAM_CHECK_RET(s32Ret, "app_ipcam_Vi_DevAttr_Get(%d) failed!\n", ViPipe);

        s32Ret = app_ipcam_Vi_ChnAttr_Get(pstSnsCfg->enSnsType, &stViChnAttr);
        APP_IPCAM_CHECK_RET(s32Ret, "app_ipcam_Vi_ChnAttr_Get(%d) failed!\n", ViPipe);

        stViChnAttr.stSize.u32Width  = pstChnCfg->u32Width;
        stViChnAttr.stSize.u32Height = pstChnCfg->u32Height;
        stViChnAttr.enCompressMode = pstChnCfg->enCompressMode;
        stViChnAttr.enPixelFormat = pstChnCfg->enPixFormat;

        stViChnAttr.u32Depth         = 0; // depth
        // stViChnAttr.bLVDSflow        = (stViDevAttr.enIntfMode == VI_MODE_LVDS) ? 1 : 0;
        // stViChnAttr.u8TotalChnNum    = vt->ViConfig.s32WorkingViNum;

        /* fill the sensor orientation */
        if (pstSnsCfg->u8Orien <= 3) {
            stViChnAttr.bMirror = pstSnsCfg->u8Orien & 0x1;
            stViChnAttr.bFlip = pstSnsCfg->u8Orien & 0x2;
        }

        s32Ret = CVI_VI_SetChnAttr(ViPipe, ViChn, &stViChnAttr);
        APP_IPCAM_CHECK_RET(s32Ret, "CVI_VI_SetChnAttr(%d) failed!\n", ViPipe);

        if (pstSnsObj && pstSnsObj->pfnMirrorFlip) {
            CVI_VI_RegChnFlipMirrorCallBack(ViPipe, ViChn, (void *)pstSnsObj->pfnMirrorFlip);
        }

        s32Ret = CVI_VI_EnableChn(ViPipe, ViChn);
        APP_IPCAM_CHECK_RET(s32Ret, "CVI_VI_EnableChn(%d) failed!\n", ViPipe);

    }
    return CVI_SUCCESS;
}

static int app_ipcam_Vi_Chn_Stop(void)
{
    CVI_S32 s32Ret;
    VI_CHN ViChn;
    VI_PIPE ViPipe;
    VI_VPSS_MODE_E enMastPipeMode;

    for (CVI_U32 i = 0; i < g_pstViCtx->u32WorkSnsCnt; i++) {
        APP_PARAM_PIPE_CFG_T *psPipeCfg = &g_pstViCtx->astPipeInfo[i];
        APP_PARAM_CHN_CFG_T *pstChnCfg = &g_pstViCtx->astChnInfo[i];
        ViPipe = pstChnCfg->s32ChnId;
        ViChn = pstChnCfg->s32ChnId;
        enMastPipeMode = psPipeCfg->enMastPipeMode;

        if (ViChn < VI_MAX_CHN_NUM) {
            CVI_VI_UnRegChnFlipMirrorCallBack(ViPipe, ViChn);

            if (enMastPipeMode == VI_OFFLINE_VPSS_OFFLINE || enMastPipeMode == VI_ONLINE_VPSS_OFFLINE) {
                s32Ret = CVI_VI_DisableChn(ViPipe, ViChn);
                if (s32Ret != CVI_SUCCESS) {
                    APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_VI_DisableChn failed with %#x!\n", s32Ret);
                    return s32Ret;
                }
            }
        }
    }

    return CVI_SUCCESS;
}

static int app_ipcam_Vi_Close(void)
{
    CVI_S32 s32Ret = CVI_SUCCESS;

    s32Ret = CVI_SYS_VI_Close();
    if (s32Ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_SYS_VI_Close failed with %#x!\n", s32Ret);
    }

    return s32Ret;
}

int app_ipcam_Vi_DeInit(void)
{
    APP_CHK_RET(app_ipcam_Vi_Isp_Stop(),  "app_ipcam_Vi_Isp_Stop");
    APP_CHK_RET(app_ipcam_Vi_Isp_DeInit(),  "app_ipcam_Vi_Isp_DeInit");
    APP_CHK_RET(app_ipcam_Vi_Chn_Stop(),  "app_ipcam_Vi_Chn_Stop");
    APP_CHK_RET(app_ipcam_Vi_Pipe_Stop(), "app_ipcam_Vi_Pipe_Stop");
    APP_CHK_RET(app_ipcam_Vi_Dev_Stop(),  "app_ipcam_Vi_Dev_Stop");
    APP_CHK_RET(app_ipcam_Vi_Close(),  "app_ipcam_Vi_Close");

    return CVI_SUCCESS;
}

int app_ipcam_Vi_Init(void)
{
    CVI_S32 s32Ret = CVI_SUCCESS;

    s32Ret = CVI_SYS_VI_Open();
    if (s32Ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_SYS_VI_Open failed with %#x\n", s32Ret);
        return s32Ret;
    }

    s32Ret = app_ipcam_Vi_Sensor_Start();
    if(s32Ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "app_ipcam_Vi_Sensor_Start failed with %#x\n", s32Ret);
        goto VI_EXIT0;
    }

    s32Ret = app_ipcam_Vi_Mipi_Start();
    if(s32Ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "app_ipcam_Mipi_Start failed with %#x\n", s32Ret);
        goto VI_EXIT0;
    }

    s32Ret = app_ipcam_Vi_Dev_Start();
    if(s32Ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "app_ipcam_Dev_Start failed with %#x\n", s32Ret);
        goto VI_EXIT0;
    }

    s32Ret = app_ipcam_Vi_Pipe_Start();
    if(s32Ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "app_ipcam_Pipe_Start failed with %#x\n", s32Ret);
        goto VI_EXIT1;
    }

    s32Ret = app_ipcam_Vi_Isp_Init();
    if(s32Ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "app_ipcam_Isp_Init failed with %#x\n", s32Ret);
        goto VI_EXIT2;
    }

    s32Ret = app_ipcam_Vi_Isp_Start();
    if(s32Ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "app_ipcam_Isp_Start failed with %#x\n", s32Ret);
        goto VI_EXIT3;
    }

    s32Ret = app_ipcam_Vi_Chn_Start();
    if(s32Ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "app_ipcam_Chn_Start failed with %#x\n", s32Ret);
        goto VI_EXIT4;
    }

    return CVI_SUCCESS;

VI_EXIT4:
    app_ipcam_Vi_Isp_Stop();

VI_EXIT3:
    app_ipcam_Vi_Isp_DeInit();

VI_EXIT2:
    app_ipcam_Vi_Pipe_Stop();

VI_EXIT1:
    app_ipcam_Vi_Dev_Stop();

VI_EXIT0:
    app_ipcam_Vi_Close();
    app_ipcam_Sys_DeInit();

    return s32Ret;
}
