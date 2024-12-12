#ifndef __APP_IPCAM_VI_H__
#define __APP_IPCAM_VI_H__

#include <cvi_isp.h>
#include <cvi_mipi.h>
#include <cvi_sns_ctrl.h>
#include <cvi_sys.h>
#include <cvi_vi.h>

#include "app_ipcam_isp.h"
#include "app_ipcam_sensors.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define WDR_MAX_PIPE_NUM 4 //need checking by jammy

typedef struct APP_PARAM_VI_PM_DATA_T {
	VI_PIPE ViPipe;
	CVI_U32 u32SnsId;
	CVI_S32 s32DevNo;
} APP_PARAM_VI_PM_DATA_S;

typedef enum {
    ISP_PROC_LOG_LEVEL_NONE,
    ISP_PROC_LOG_LEVEL_1,
    ISP_PROC_LOG_LEVEL_2,
    ISP_PROC_LOG_LEVEL_3,
    ISP_PROC_LOG_LEVEL_MAX,
} ISP_PROC_LOG_LEVEL_E;

typedef struct APP_PARAM_SNS_CFG_S {
    CVI_S32 s32SnsId;
    SENSOR_TYPE_E enSnsType;
    WDR_MODE_E enWDRMode;
    CVI_S32 s32Framerate;
    CVI_S32 s32BusId;
    CVI_S32 s32I2cAddr;
    combo_dev_t MipiDev;
    CVI_S16 as16LaneId[5];
    CVI_S8  as8PNSwap[5];
    CVI_BOOL bMclkEn;
    CVI_U8 u8Mclk;
    CVI_S32 u8Orien;
    CVI_BOOL bHwSync;
    CVI_U8 u8UseDualSns;
}APP_PARAM_SNS_CFG_T;

typedef struct APP_PARAM_DEV_CFG_S {
    VI_DEV ViDev;
    WDR_MODE_E enWDRMode;
} APP_PARAM_DEV_CFG_T;

typedef struct APP_PARAM_PIPE_CFG_S {
    VI_PIPE aPipe[WDR_MAX_PIPE_NUM];
    VI_VPSS_MODE_E enMastPipeMode;
    // bool bMultiPipe;
    // bool bVcNumCfged;
    // bool bIspBypass;
    // PIXEL_FORMAT_E enPixFmt;
    // CVI_U32 u32VCNum[WDR_MAX_PIPE_NUM];
} APP_PARAM_PIPE_CFG_T;

typedef struct APP_PARAM_CHN_CFG_S {
    CVI_S32 s32ChnId;
    CVI_U32 u32Width;
    CVI_U32 u32Height;
    CVI_FLOAT f32Fps;
    PIXEL_FORMAT_E enPixFormat;
    WDR_MODE_E enWDRMode;
    DYNAMIC_RANGE_E enDynamicRange;
    VIDEO_FORMAT_E enVideoFormat;
    COMPRESS_MODE_E enCompressMode;
} APP_PARAM_CHN_CFG_T;

typedef struct APP_PARAM_SNAP_INFO_S {
    bool bSnap;
    bool bDoublePipe;
    VI_PIPE VideoPipe;
    VI_PIPE SnapPipe;
    VI_VPSS_MODE_E enVideoPipeMode;
    VI_VPSS_MODE_E enSnapPipeMode;
} APP_PARAM_SNAP_INFO_T;

typedef struct APP_PARAM_ISP_CFG_T {
    CVI_BOOL bAfFliter;
} APP_PARAM_ISP_CFG_S;

typedef struct APP_PARAM_VI_CFG_T {
    CVI_U32 u32WorkSnsCnt;
    APP_PARAM_SNS_CFG_T astSensorCfg[VI_MAX_DEV_NUM];
    APP_PARAM_DEV_CFG_T astDevInfo[VI_MAX_DEV_NUM];
    APP_PARAM_PIPE_CFG_T astPipeInfo[VI_MAX_DEV_NUM];
    APP_PARAM_CHN_CFG_T astChnInfo[VI_MAX_DEV_NUM];
    // APP_PARAM_SNAP_INFO_T astSnapInfo[VI_MAX_DEV_NUM];
    APP_PARAM_ISP_CFG_S astIspCfg[VI_MAX_DEV_NUM];
    // CVI_U32 u32Depth;
    // SIZE_S stSize;
} APP_PARAM_VI_CTX_S;

APP_PARAM_VI_CTX_S *app_ipcam_Vi_Param_Get(void);
int app_ipcam_Vi_Init(void);
int app_ipcam_Vi_DeInit(void);

#ifdef __cplusplus
}
#endif

#endif
