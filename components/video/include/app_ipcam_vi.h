#ifndef __APP_IPCAM_VI_H__
#define __APP_IPCAM_VI_H__

#include "cvi_sns_ctrl.h"
#include "cvi_comm_isp.h"
#include "cvi_comm_3a.h"
#include "cvi_comm_sns.h"
#include "cvi_mipi.h"
#include "cvi_isp.h"
#include "cvi_vi.h"
#include "cvi_sys.h"
#include "app_ipcam_mq.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define WDR_MAX_PIPE_NUM 4 //need checking by jammy

#ifdef SUPPORT_ISP_PQTOOL
#define PQ_BIN_SDR          "/mnt/data/cvi_sdr_bin"
#define PQ_BIN_IR           "/mnt/data/cvi_sdr_ir_bin"
#define PQ_BIN_NIGHT_COLOR  "/mnt/data/cvi_night_color_bin"
#define PQ_BIN_WDR          "/mnt/data/cvi_wdr_bin"
#else
#define PQ_BIN_SDR          "/mnt/cfg/param/cvi_sdr_bin"
#define PQ_BIN_IR           "/mnt/cfg/param/cvi_sdr_ir_bin"
#define PQ_BIN_NIGHT_COLOR  "/mnt/cfg/param/cvi_night_color_bin"
#define PQ_BIN_WDR          "/mnt/cfg/param/cvi_wdr_bin"
#endif

typedef enum _SENSOR_TYPE_E {
    SENSOR_NONE,
    SENSOR_GCORE_GC1054,
    SENSOR_GCORE_GC2053,
    SENSOR_GCORE_GC2053_1L,
    SENSOR_GCORE_GC2053_SLAVE,
    SENSOR_GCORE_GC2093,
    SENSOR_GCORE_GC2093_SLAVE,
    SENSOR_GCORE_GC4653,
    SENSOR_GCORE_GC4023,
    SENSOR_GCORE_GC4653_SLAVE,
    SENSOR_NEXTCHIP_N5,
    SENSOR_NEXTCHIP_N6,
    SENSOR_OV_OS08A20,
    SENSOR_OV_OS08A20_SLAVE,
    SENSOR_OV_OV5647,
    SENSOR_PICO_384,
    SENSOR_PICO_640,
    SENSOR_PIXELPLUS_PR2020,
    SENSOR_PIXELPLUS_PR2100,
    SENSOR_SMS_SC1346_1L,
    SENSOR_SMS_SC1346_1L_60,
    SENSOR_SMS_SC200AI,
    SENSOR_SMS_SC2331_1L,
    SENSOR_SMS_SC2335,
    SENSOR_SMS_SC2336,
    SENSOR_SMS_SC2336P,
    SENSOR_SMS_SC3335,
    SENSOR_SMS_SC3335_SLAVE,
    SENSOR_SMS_SC3336,  
    SENSOR_SMS_SC401AI,
    SENSOR_SMS_SC4210,
    SENSOR_SMS_SC8238,
    SENSOR_SMS_SC531AI_2L,
    SENSOR_SMS_SC5336_2L,
    SENSOR_SMS_SC4336P,
    SENSOR_SOI_F23,
    SENSOR_SOI_F35,
    SENSOR_SOI_F35_SLAVE,
    SENSOR_SOI_H65,
    SENSOR_SOI_K06,
    SENSOR_SOI_Q03P,
    SENSOR_SONY_IMX290_2L,
    SENSOR_SONY_IMX307,
    SENSOR_SONY_IMX307_2L,
    SENSOR_SONY_IMX307_SLAVE,
    SENSOR_SONY_IMX307_SUBLVDS,
    SENSOR_SONY_IMX327,
    SENSOR_SONY_IMX327_2L,
    SENSOR_SONY_IMX327_SLAVE,
    SENSOR_SONY_IMX327_SUBLVDS,
    SENSOR_SONY_IMX334,
    SENSOR_SONY_IMX335,
    SENSOR_SONY_IMX347,
    SENSOR_SONY_IMX385,
    SENSOR_VIVO_MCS369,
    SENSOR_VIVO_MCS369Q,
    SENSOR_VIVO_MM308M2,
    SENSOR_IMGDS_MIS2008,
    SENSOR_IMGDS_MIS2008_1L,
    SENSOR_BUTT
} SENSOR_TYPE_E;

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
    bool bMultiPipe;
    bool bVcNumCfged;
    bool bIspBypass;
    PIXEL_FORMAT_E enPixFmt;
    CVI_U32 u32VCNum[WDR_MAX_PIPE_NUM];
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
    APP_PARAM_SNAP_INFO_T astSnapInfo[VI_MAX_DEV_NUM];
    APP_PARAM_ISP_CFG_S astIspCfg[VI_MAX_DEV_NUM];
    CVI_U32 u32Depth;
    SIZE_S stSize;
} APP_PARAM_VI_CTX_S;

APP_PARAM_VI_CTX_S *app_ipcam_Vi_Param_Get(void);
int app_ipcam_Vi_Init(void);
int app_ipcam_Vi_DeInit(void);
int app_ipcam_PQBin_Load(const CVI_CHAR *pBinPath);
void app_ipcam_Framerate_Set(CVI_U8 viPipe, CVI_U8 fps);
CVI_U8 app_ipcam_Framerate_Get(CVI_U8 viPipe);

int app_ipcam_CmdTask_Auto_Rgb_Ir_Switch(CVI_MQ_MSG_t *msg, CVI_VOID *userdate);
int app_ipcam_CmdTask_Setect_Pq(CVI_MQ_MSG_t *msg, CVI_VOID *userdate);
int app_ipcam_CmdTask_Mirror_Switch(CVI_MQ_MSG_t *msg, CVI_VOID *userdate);
int app_ipcam_CmdTask_Flip_Switch(CVI_MQ_MSG_t *msg, CVI_VOID *userdate);
int app_ipcam_CmdTask_Flip_Mirror_Switch(CVI_MQ_MSG_t *msg, CVI_VOID *userdate);

#ifdef __cplusplus
}
#endif

#endif
