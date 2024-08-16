#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <pthread.h>
#include <sys/prctl.h>
#include <sys/time.h>
#include <unistd.h>
#include "linux/cvi_comm_sys.h"
#include "cvi_bin.h"
#include "cvi_ae.h"
#include "cvi_awb.h"
#include "app_ipcam_vi.h"
#include "cvi_ispd2.h"
#include "app_ipcam_paramparse.h"
#ifdef IRCUT_SUPPORT
#include "app_ipcam_ircut.h"
#endif
#ifdef SUPPORT_ISP_PQTOOL
#include <dlfcn.h>
#endif


/**************************************************************************
 *                              M A C R O S                               *
 **************************************************************************/
#ifdef SUPPORT_ISP_PQTOOL
#define ISPD_LIBNAME "libcvi_ispd2.so"
#define ISPD_CONNECT_PORT 5566
/* 183x not support continuous RAW dump */
#ifndef ARCH_CV183X
#define RAW_DUMP_LIBNAME "libraw_dump.so"
#endif
#endif

/**************************************************************************
 *                           C O N S T A N T S                            *
 **************************************************************************/

/**************************************************************************
 *                          D A T A    T Y P E S                          *
 **************************************************************************/

/**************************************************************************
 *                         G L O B A L    D A T A                         *
 **************************************************************************/

APP_PARAM_VI_CTX_S g_stViCtx, *g_pstViCtx = &g_stViCtx;

static pthread_t AF_pthread;
static CVI_BOOL bAfFilterEnable;
static pthread_t g_IspPid[VI_MAX_DEV_NUM];

#ifdef SUPPORT_ISP_PQTOOL
static CVI_BOOL bISPDaemon = CVI_FALSE;
//static CVI_VOID *pISPDHandle = NULL;
#ifndef ARCH_CV183X
static CVI_BOOL bRawDump = CVI_FALSE;
static CVI_VOID *pRawDumpHandle = NULL;
#endif
#endif

APP_PARAM_VI_PM_DATA_S ViPmData[VI_MAX_DEV_NUM] = { 0 };

VI_DEV_ATTR_S vi_dev_attr_base = {
    .enIntfMode = VI_MODE_MIPI,
    .enWorkMode = VI_WORK_MODE_1Multiplex,
    .enScanMode = VI_SCAN_PROGRESSIVE,
    .as32AdChnId = {-1, -1, -1, -1},
    .enDataSeq = VI_DATA_SEQ_YUYV,
    .stSynCfg = {
    /*port_vsync    port_vsync_neg    port_hsync              port_hsync_neg*/
    VI_VSYNC_PULSE, VI_VSYNC_NEG_LOW, VI_HSYNC_VALID_SINGNAL, VI_HSYNC_NEG_HIGH,
    /*port_vsync_valid     port_vsync_valid_neg*/
    VI_VSYNC_VALID_SIGNAL, VI_VSYNC_VALID_NEG_HIGH,

    /*hsync_hfb  hsync_act  hsync_hhb*/
    {0,           1920,       0,
    /*vsync0_vhb vsync0_act vsync0_hhb*/
     0,           1080,       0,
    /*vsync1_vhb vsync1_act vsync1_hhb*/
     0,            0,         0}
    },
    .enInputDataType = VI_DATA_TYPE_RGB,
    .stSize = {1920, 1080},
    .stWDRAttr = {WDR_MODE_NONE, 1080},
    .enBayerFormat = BAYER_FORMAT_BG,
};

VI_PIPE_ATTR_S vi_pipe_attr_base = {
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
    .stFrameRate = {-1, -1},
    .bDiscardProPic = CVI_FALSE,
    .bYuvBypassPath = CVI_FALSE,
};

VI_CHN_ATTR_S vi_chn_attr_base = {
    .stSize = {1920, 1080},
    .enPixelFormat = PIXEL_FORMAT_YUV_PLANAR_420,
    .enDynamicRange = DYNAMIC_RANGE_SDR8,
    .enVideoFormat = VIDEO_FORMAT_LINEAR,
    .enCompressMode = COMPRESS_MODE_NONE,
    .bMirror = CVI_FALSE,
    .bFlip = CVI_FALSE,
    .u32Depth = 0,
    .stFrameRate = {-1, -1},
};

ISP_PUB_ATTR_S isp_pub_attr_base = {
    .stWndRect = {0, 0, 1920, 1080},
    .stSnsSize = {1920, 1080},
    .f32FrameRate = 25.0f,
    .enBayer = BAYER_BGGR,
    .enWDRMode = WDR_MODE_NONE,
    .u8SnsMode = 0,
};

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

static CVI_S32 app_ipcam_Isp_AfFilter_Init(CVI_VOID)
{
    CVI_S32 s32Ret = CVI_SUCCESS;
    VI_PIPE ViPipe = 0;
    //CVI_CHAR input[10];
    ISP_STATISTICS_CFG_S stsCfg;
    ISP_PUB_ATTR_S stPubAttr;

    ISP_CTRL_PARAM_S stIspCtrlParam;
    CVI_ISP_GetCtrlParam(ViPipe, &stIspCtrlParam);
    stIspCtrlParam.u32AFStatIntvl = 1;
    CVI_ISP_SetCtrlParam(ViPipe, &stIspCtrlParam);

    // Get current statistic and related size setting.
    s32Ret = CVI_ISP_GetStatisticsConfig(ViPipe, &stsCfg);
    s32Ret |= CVI_ISP_GetPubAttr(ViPipe, &stPubAttr);
    if (s32Ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "Get Statistic info fail with %#x\n", s32Ret);
        return s32Ret;
    }
    // Config AF Enable.
    stsCfg.stFocusCfg.stConfig.bEnable = 1;
    // Config low pass filter.
    stsCfg.stFocusCfg.stConfig.u8HFltShift = 0;
    stsCfg.stFocusCfg.stConfig.s8HVFltLpCoeff[0] = 0;
    stsCfg.stFocusCfg.stConfig.s8HVFltLpCoeff[1] = 1;
    stsCfg.stFocusCfg.stConfig.s8HVFltLpCoeff[2] = 2;
    stsCfg.stFocusCfg.stConfig.s8HVFltLpCoeff[3] = 3;
    stsCfg.stFocusCfg.stConfig.s8HVFltLpCoeff[4] = 4;
    // Config gamma enable.
    stsCfg.stFocusCfg.stConfig.stRawCfg.PreGammaEn  = 0;
    // Config pre NR enable.
    stsCfg.stFocusCfg.stConfig.stPreFltCfg.PreFltEn = 1;
    // Config H & V window.
    stsCfg.stFocusCfg.stConfig.u16Hwnd = 17;
    stsCfg.stFocusCfg.stConfig.u16Vwnd = 15;
    // Config crop related setting. Has some limitation
    stsCfg.stFocusCfg.stConfig.stCrop.bEnable = 1;
    stsCfg.stFocusCfg.stConfig.stCrop.u16X = 8;
    stsCfg.stFocusCfg.stConfig.stCrop.u16Y = 2;
    stsCfg.stFocusCfg.stConfig.stCrop.u16W = stPubAttr.stWndRect.u32Width - 8 * 2;
    stsCfg.stFocusCfg.stConfig.stCrop.u16H = stPubAttr.stWndRect.u32Height - 2 * 2;
    // Config first horizontal high pass filter.
    stsCfg.stFocusCfg.stHParam_FIR0.s8HFltHpCoeff[0] = 0;
    stsCfg.stFocusCfg.stHParam_FIR0.s8HFltHpCoeff[1] = 0;
    stsCfg.stFocusCfg.stHParam_FIR0.s8HFltHpCoeff[2] = 13;
    stsCfg.stFocusCfg.stHParam_FIR0.s8HFltHpCoeff[3] = 24;
    stsCfg.stFocusCfg.stHParam_FIR0.s8HFltHpCoeff[4] = 0;
    // Config 2nd horizontal high pass filter.
    stsCfg.stFocusCfg.stHParam_FIR1.s8HFltHpCoeff[0] = 1;
    stsCfg.stFocusCfg.stHParam_FIR1.s8HFltHpCoeff[1] = 2;
    stsCfg.stFocusCfg.stHParam_FIR1.s8HFltHpCoeff[2] = 4;
    stsCfg.stFocusCfg.stHParam_FIR1.s8HFltHpCoeff[3] = 8;
    stsCfg.stFocusCfg.stHParam_FIR1.s8HFltHpCoeff[4] = 0;

    // Config vertical high pass filter.
    stsCfg.stFocusCfg.stVParam_FIR.s8VFltHpCoeff[0] = 8;
    stsCfg.stFocusCfg.stVParam_FIR.s8VFltHpCoeff[1] = -15;
    stsCfg.stFocusCfg.stVParam_FIR.s8VFltHpCoeff[2 ] = 0;
    stsCfg.unKey.bit1FEAfStat = 1;

    // Config high luma thr
    stsCfg.stFocusCfg.stConfig.u16HighLumaTh = 3800;

    //LDG
    // stsCfg.stFocusCfg.stConfig.u8ThLow = 0;
    // stsCfg.stFocusCfg.stConfig.u8ThHigh = 255;
    // stsCfg.stFocusCfg.stConfig.u8GainLow = 30;
    // stsCfg.stFocusCfg.stConfig.u8GainHigh = 20;
    // stsCfg.stFocusCfg.stConfig.u8SlopLow = 8;
    // stsCfg.stFocusCfg.stConfig.u8SlopHigh = 15;

    s32Ret = CVI_ISP_SetStatisticsConfig(ViPipe, &stsCfg);
    if (s32Ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "Set Statistic info fail with %#x\n", s32Ret);
        return s32Ret;
    }

    return CVI_SUCCESS;
}

void app_ipcam_Isp_AfFilter_Get(ISP_AF_STATISTICS_S *pAfStat)
{
    CVI_U64 FVP = 0, FVPn = 0, FVQ = 0, FVQn = 0;
    CVI_U32 totalWeightSum = 0;
    CVI_U32 row, col;
    const CVI_U32 weight1 = 1, weight2 = 1, weight3 = 1;
    
    for (row = 0; row < AF_ZONE_ROW; row++) {
        for (col = 0; col < AF_ZONE_COLUMN; col++) {
            CVI_U64 h0 = pAfStat->stFEAFStat.stZoneMetrics[row][col].u64h0;
            CVI_U64 h1 = pAfStat->stFEAFStat.stZoneMetrics[row][col].u64h1;
            CVI_U32 v0 = pAfStat->stFEAFStat.stZoneMetrics[row][col].u32v0;
            FVPn = (weight1 * h1 + weight3 * v0) / (weight1 + weight3);
            FVQn = (weight1 * h0 + weight2 * v0) / (weight1 + weight2);
            FVP += FVPn;
            FVQ += FVQn;
            totalWeightSum += 1;
        }
    }

    FVP = FVP / totalWeightSum;
    FVQ = FVQ / totalWeightSum;

    APP_PROF_LOG_PRINT(LEVEL_TRACE, "FVP = %"PRIu64" FVQ = %"PRIu64"\n", FVP, FVQ);

    ISP_EXP_INFO_S stExpInfo;
    CVI_ISP_QueryExposureInfo(0, &stExpInfo);

    CVI_U8 u8P1 = FVP & 0xFF00;
    CVI_U8 u8P2 = FVP & 0x00FF;
    CVI_U8 u8Q1 = FVQ & 0xFF00;
    CVI_U8 u8Q2 = FVQ & 0x00FF;
    CVI_U8 u8R1 = log(stExpInfo.u32ISO)/log(2);

    APP_PROF_LOG_PRINT(LEVEL_DEBUG, "u32ISO=%d log(stExpInfo.u32ISO/100)/log(2)=%f\n", stExpInfo.u32ISO, log(stExpInfo.u32ISO/100)/log(2));

    APP_PROF_LOG_PRINT(LEVEL_TRACE, "R1=%d P1=%d P2=%d Q1=%d Q2=%d\n", u8R1, u8P1, u8P2, u8Q1, u8Q2);
}

static void *Thread_AF_Filter_Proc(void *pArgs)
{
    CVI_S32 s32Ret = CVI_SUCCESS;

    CVI_CHAR TaskName[64];
    sprintf(TaskName, "AF_Filter_Get");
    prctl(PR_SET_NAME, TaskName, 0, 0, 0);
    APP_PROF_LOG_PRINT(LEVEL_INFO, "Thread_AF_Filter_Proc task started \n");

    ISP_AF_STATISTICS_S afStat;
    CVI_U32 row, col;

    VI_PIPE ViPipe = 0;
    ISP_VD_TYPE_E enIspVDType = ISP_VD_FE_START;
    CVI_U64 FVn = 0, FV = 0;
    CVI_U32 totalWeightSum = 0;
    // weight for each statistic
    const CVI_U32 weight1 = 1, weight2 = 1, weight3 = 1;
    const CVI_U32 blockWeightSum = weight1 + weight2 + weight3;

    while (bAfFilterEnable) {
        s32Ret = CVI_ISP_GetVDTimeOut(ViPipe, enIspVDType, 5000);
        s32Ret |= CVI_ISP_GetFocusStatistics(ViPipe, &afStat);
        if (s32Ret != CVI_SUCCESS) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "Get Statistic failed with %#x\n", s32Ret);
            continue;
        }
        FVn = 0, FV = 0;
        totalWeightSum = 0;
        // calculate AF statistics
        for (row = 0; row < AF_ZONE_ROW; row++) {
            for (col = 0; col < AF_ZONE_COLUMN; col++) {
                CVI_U64 h0 = afStat.stFEAFStat.stZoneMetrics[row][col].u64h0;
                CVI_U64 h1 = afStat.stFEAFStat.stZoneMetrics[row][col].u64h1;
                CVI_U32 v0 = afStat.stFEAFStat.stZoneMetrics[row][col].u32v0;
                FVn = (weight1 * h0 + weight2 * h1 + weight3 * v0) / blockWeightSum;
                FV += FVn;
                totalWeightSum += 1;
            }
        }
        
        FV = FV / totalWeightSum;

        CVI_U32 u32Fv = FV & 0xFFFFFFFF;

        APP_PROF_LOG_PRINT(LEVEL_TRACE, "FV = %"PRIu64", u32Fv = %u\n", FV, u32Fv);

        /* for customer used */
        app_ipcam_Isp_AfFilter_Get(&afStat);
    }

    return (void *) CVI_SUCCESS;
}


int app_ipcam_Isp_AfFilter_Start(void) 
{
    CVI_S32 s32Ret = CVI_SUCCESS;

    s32Ret = app_ipcam_Isp_AfFilter_Init();
    if(s32Ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "app_ipcam_Isp_AfFilter_Start failed with %#x\n", s32Ret);
        return CVI_FAILURE;
    }

    pthread_attr_t pthread_attr;
    pthread_attr_init(&pthread_attr);

    s32Ret = pthread_create(
                    &AF_pthread,
                    &pthread_attr,
                    Thread_AF_Filter_Proc,
                    NULL);
    if (s32Ret) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "AF filter pthread_create failed:0x%x\n", s32Ret);
        return CVI_FAILURE;
    }

    return CVI_SUCCESS;
}

static CVI_S32 app_ipcam_ISP_ProcInfo_Open(CVI_U32 ProcLogLev)
{
    CVI_S32 s32Ret = CVI_SUCCESS;

    if (ProcLogLev == ISP_PROC_LOG_LEVEL_NONE) {
        APP_PROF_LOG_PRINT(LEVEL_INFO, "isp proc log not enable\n");
    } else {
        ISP_CTRL_PARAM_S setParam;
        memset(&setParam, 0, sizeof(ISP_CTRL_PARAM_S));

        setParam.u32ProcLevel = ProcLogLev;	// proc printf level (level =0,disable; =3,log max)
        setParam.u32ProcParam = 15;		// isp info frequency of collection (unit:frame; rang:(0,0xffffffff])
        setParam.u32AEStatIntvl = 1;	// AE info update frequency (unit:frame; rang:(0,0xffffffff])
        setParam.u32AWBStatIntvl = 6;	// AW info update frequency (unit:frame; rang:(0,0xffffffff])
        setParam.u32AFStatIntvl = 1;	// AF info update frequency (unit:frame; rang:(0,0xffffffff])
        setParam.u32UpdatePos = 0;		// Now, only support before sensor cfg; default 0
        setParam.u32IntTimeOut = 0;		// interrupt timeout; unit:ms; not used now
        setParam.u32PwmNumber = 0;		// PWM Num ID; Not used now
        setParam.u32PortIntDelay = 0;	// Port interrupt delay time

        s32Ret = CVI_ISP_SetCtrlParam(0, &setParam);
        if (s32Ret != CVI_SUCCESS) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_ISP_SetCtrlParam failed with %#x\n", s32Ret);
            return s32Ret;
        }
    }

    return s32Ret;
}

#ifdef SUPPORT_ISP_PQTOOL
static CVI_VOID app_ipcam_Ispd_Load(CVI_VOID)
{
    // char *dlerr = NULL;
    // UNUSED(dlerr);
    if (!bISPDaemon) {
        isp_daemon2_init(ISPD_CONNECT_PORT);
        APP_PROF_LOG_PRINT(LEVEL_INFO, "Isp_daemon2_init %d success\n", ISPD_CONNECT_PORT);
        bISPDaemon = CVI_TRUE;
        //pISPDHandle = dlopen(ISPD_LIBNAME, RTLD_NOW);
        
        // dlerr = dlerror();
        // if (pISPDHandle) {
        //     APP_PROF_LOG_PRINT(LEVEL_INFO, "Load dynamic library %s success\n", ISPD_LIBNAME);

        //     void (*daemon_init)(unsigned int port);
        //     daemon_init = dlsym(pISPDHandle, "isp_daemon2_init");

        //     dlerr = dlerror();
        //     if (dlerr == NULL) {
        //         (*daemon_init)(ISPD_CONNECT_PORT);
        //         bISPDaemon = CVI_TRUE;
        //     } else {
        //         APP_PROF_LOG_PRINT(LEVEL_ERROR, "Run daemon initial failed with %s\n", dlerr);
        //         dlclose(pISPDHandle);
        //         pISPDHandle = NULL;
        //     }
        // } else {
        //     APP_PROF_LOG_PRINT(LEVEL_ERROR, "Load dynamic library %s failed with %s\n", ISPD_LIBNAME, dlerr);
        // }
    } else {
        APP_PROF_LOG_PRINT(LEVEL_WARN, "%s already loaded\n", ISPD_LIBNAME);
    }
}

static CVI_VOID app_ipcam_Ispd_Unload(CVI_VOID)
{
    if (bISPDaemon) {
        // void (*daemon_uninit)(void);

        // daemon_uninit = dlsym(pISPDHandle, "isp_daemon_uninit");
        // if (dlerror() == NULL) {
        //     (*daemon_uninit)();
        // }

        // dlclose(pISPDHandle);
        // pISPDHandle = NULL;
        isp_daemon2_uninit();
        
        bISPDaemon = CVI_FALSE;
    } else {
        APP_PROF_LOG_PRINT(LEVEL_WARN, "%s not load yet!\n", ISPD_LIBNAME);
    }
}

#ifndef ARCH_CV183X
static CVI_VOID app_ipcam_RawDump_Load(void)
{
    char *dlerr = NULL;

    if (!bRawDump) {
        pRawDumpHandle = dlopen(RAW_DUMP_LIBNAME, RTLD_NOW);
                
        dlerr = dlerror();

        if (pRawDumpHandle) {
            void (*cvi_raw_dump_init)(void);

            APP_PROF_LOG_PRINT(LEVEL_INFO, "Load dynamic library %s success\n", RAW_DUMP_LIBNAME);

            cvi_raw_dump_init = dlsym(pRawDumpHandle, "cvi_raw_dump_init");
            
            dlerr = dlerror();
            if (dlerr == NULL) {
                (*cvi_raw_dump_init)();
                bRawDump = CVI_TRUE;
            } else {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "Run raw dump initial fail, %s\n", dlerr);
                dlclose(pRawDumpHandle);
                pRawDumpHandle = NULL;
            }
        } else {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "dlopen: %s, dlerr: %s\n", RAW_DUMP_LIBNAME, dlerr);        
        }    
    } else {
        APP_PROF_LOG_PRINT(LEVEL_WARN, "%s already loaded\n", RAW_DUMP_LIBNAME);
    }
}

static CVI_VOID app_ipcam_RawDump_Unload(CVI_VOID)
{
    if (bRawDump) {
        void (*daemon_uninit)(void);

        daemon_uninit = dlsym(pRawDumpHandle, "isp_daemon_uninit");
        if (dlerror() == NULL) {
            (*daemon_uninit)();
        }

        dlclose(pRawDumpHandle);
        pRawDumpHandle = NULL;
        bRawDump = CVI_FALSE;
    }
}
#endif
#endif

static ISP_SNS_OBJ_S *app_ipcam_SnsObj_Get(SENSOR_TYPE_E enSnsType)
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

CVI_S32 app_ipcam_Vi_DevAttr_Get(SENSOR_TYPE_E enSnsType, VI_DEV_ATTR_S *pstViDevAttr)
{
    CVI_S32 s32Ret = CVI_SUCCESS;

    memcpy(pstViDevAttr, &vi_dev_attr_base, sizeof(VI_DEV_ATTR_S));

    switch (enSnsType) {
    case SENSOR_GCORE_GC1054:
    case SENSOR_GCORE_GC2053:
    case SENSOR_GCORE_GC2053_1L:
    case SENSOR_GCORE_GC2053_SLAVE:
    case SENSOR_GCORE_GC2093:
    case SENSOR_GCORE_GC4023:
    case SENSOR_GCORE_GC2093_SLAVE:
    case SENSOR_OV_OV5647:
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

CVI_S32 app_ipcam_Vi_PipeAttr_Get(SENSOR_TYPE_E enSnsType, VI_PIPE_ATTR_S *pstViPipeAttr)
{
    CVI_S32 s32Ret = CVI_SUCCESS;

    memcpy(pstViPipeAttr, &vi_pipe_attr_base, sizeof(VI_PIPE_ATTR_S));

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

CVI_S32 app_ipcam_Vi_ChnAttr_Get(SENSOR_TYPE_E enSnsType, VI_CHN_ATTR_S *pstViChnAttr)
{
    CVI_S32 s32Ret = CVI_SUCCESS;

    memcpy(pstViChnAttr, &vi_chn_attr_base, sizeof(VI_CHN_ATTR_S));

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

CVI_S32 app_ipcam_Isp_InitAttr_Get(SENSOR_TYPE_E enSnsType, WDR_MODE_E enWDRMode, ISP_INIT_ATTR_S *pstIspInitAttr)
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

CVI_S32 app_ipcam_Isp_PubAttr_Get(SENSOR_TYPE_E enSnsType, ISP_PUB_ATTR_S *pstIspPubAttr)
{
    CVI_S32 s32Ret = CVI_SUCCESS;

    memcpy(pstIspPubAttr, &isp_pub_attr_base, sizeof(ISP_PUB_ATTR_S));
    //FPS
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

int app_ipcam_Vi_framerate_Set(VI_PIPE ViPipe, CVI_S32 framerate)
{
    ISP_PUB_ATTR_S pubAttr = {0};

    CVI_ISP_GetPubAttr(ViPipe, &pubAttr);

    pubAttr.f32FrameRate = (CVI_FLOAT)framerate;

    APP_CHK_RET(CVI_ISP_SetPubAttr(ViPipe, &pubAttr), "set vi framerate");

    return CVI_SUCCESS;
}

int app_ipcam_Vi_Sensor_Start(void)
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

            s32Ret = isp_sensor_exp_func.pfn_cmos_set_image_mode(ViPipe, &isp_cmos_sensor_image_mode);
            APP_IPCAM_CHECK_RET(s32Ret, "pfn_cmos_set_image_mode(%d) failed!\n", ViPipe);

            s32Ret = isp_sensor_exp_func.pfn_cmos_set_wdr_mode(ViPipe, pstChnCfg->enWDRMode);
            APP_IPCAM_CHECK_RET(s32Ret, "pfn_cmos_set_wdr_mode(%d) failed!\n", ViPipe);
        }
    }

    return CVI_SUCCESS;
}

int app_ipcam_Vi_Mipi_Start(void)
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

int app_ipcam_Vi_Dev_Start(void)
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

int app_ipcam_Vi_Dev_Stop(void)
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

int app_ipcam_Vi_Pipe_Start(void)
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

int app_ipcam_PQBin_Load(const CVI_CHAR *pBinPath)
{
    CVI_S32 ret = CVI_SUCCESS;
    FILE *fp = NULL;
    CVI_U8 *buf = NULL;
    CVI_U64 file_size;

    fp = fopen((const CVI_CHAR *)pBinPath, "rb");
    if (fp == NULL) {
        APP_PROF_LOG_PRINT(LEVEL_INFO, "Can't find bin(%s), use default parameters\n", pBinPath);
        return CVI_FAILURE;
    }

    fseek(fp, 0L, SEEK_END);
    file_size = ftell(fp);
    rewind(fp);

    buf = (CVI_U8 *)malloc(file_size);
    if (buf == NULL) {
        APP_PROF_LOG_PRINT(LEVEL_INFO, "%s\n", "Allocae memory fail");
        fclose(fp);
        return CVI_FAILURE;
    }

    fread(buf, file_size, 1, fp);

    if (fp != NULL) {
        fclose(fp);
    }
    ret = CVI_BIN_ImportBinData(buf, (CVI_U32)file_size);
    if (ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_INFO, "CVI_BIN_ImportBinData error! value:(0x%x)\n", ret);
        free(buf);
        return CVI_FAILURE;
    }

    free(buf);

    return CVI_SUCCESS;
}

int app_ipcam_Vi_Pipe_Stop(void)
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

void app_ipcam_Framerate_Set(CVI_U8 viPipe, CVI_U8 fps)
{
    ISP_PUB_ATTR_S stPubAttr;

    memset(&stPubAttr, 0, sizeof(stPubAttr));

    CVI_ISP_GetPubAttr(viPipe, &stPubAttr);

    stPubAttr.f32FrameRate = fps;

    printf("set pipe: %d, fps: %d\n", viPipe, fps);

    CVI_ISP_SetPubAttr(viPipe, &stPubAttr);
}

CVI_U8 app_ipcam_Framerate_Get(CVI_U8 viPipe)
{
    ISP_PUB_ATTR_S stPubAttr;

    memset(&stPubAttr, 0, sizeof(stPubAttr));

    CVI_ISP_GetPubAttr(viPipe, &stPubAttr);

    return stPubAttr.f32FrameRate;
}

int app_ipcam_Vi_Isp_Init(void)
{
    CVI_S32 s32Ret;
    VI_PIPE              ViPipe;
    ISP_PUB_ATTR_S       stPubAttr;
    ISP_STATISTICS_CFG_S stsCfg;
    ISP_BIND_ATTR_S      stBindAttr;
    ALG_LIB_S            stAeLib;
    ALG_LIB_S            stAwbLib;

    for (CVI_U32 i = 0; i < g_pstViCtx->u32WorkSnsCnt; i++) {

        APP_PARAM_SNS_CFG_T *pstSnsCfg = &g_pstViCtx->astSensorCfg[i];
        APP_PARAM_CHN_CFG_T *pstChnCfg = &g_pstViCtx->astChnInfo[i];
        ViPipe = pstChnCfg->s32ChnId;

        stAeLib.s32Id = ViPipe;
        strcpy(stAeLib.acLibName, CVI_AE_LIB_NAME);//, sizeof(CVI_AE_LIB_NAME));
        s32Ret = CVI_AE_Register(ViPipe, &stAeLib);
        APP_IPCAM_CHECK_RET(s32Ret, "AE Algo register fail, ViPipe[%d]\n", ViPipe);

        stAwbLib.s32Id = ViPipe;
        strcpy(stAwbLib.acLibName, CVI_AWB_LIB_NAME);//, sizeof(CVI_AWB_LIB_NAME));
        s32Ret = CVI_AWB_Register(ViPipe, &stAwbLib);
        APP_IPCAM_CHECK_RET(s32Ret, "AWB Algo register fail, ViPipe[%d]\n", ViPipe);

        memset(&stBindAttr, 0, sizeof(ISP_BIND_ATTR_S));
        stBindAttr.sensorId = 0;
        snprintf(stBindAttr.stAeLib.acLibName, sizeof(CVI_AE_LIB_NAME), "%s", CVI_AE_LIB_NAME);
        stBindAttr.stAeLib.s32Id = ViPipe;
        snprintf(stBindAttr.stAwbLib.acLibName, sizeof(CVI_AWB_LIB_NAME), "%s", CVI_AWB_LIB_NAME);
        stBindAttr.stAwbLib.s32Id = ViPipe;
        s32Ret = CVI_ISP_SetBindAttr(ViPipe, &stBindAttr);
        APP_IPCAM_CHECK_RET(s32Ret, "Bind Algo fail, ViPipe[%d]\n", ViPipe);

        s32Ret = CVI_ISP_MemInit(ViPipe);
        APP_IPCAM_CHECK_RET(s32Ret, "Init Ext memory fail, ViPipe[%d]\n", ViPipe);

        s32Ret = app_ipcam_Isp_PubAttr_Get(pstSnsCfg->enSnsType, &stPubAttr);
        APP_IPCAM_CHECK_RET(s32Ret, "app_ipcam_Isp_PubAttr_Get(%d) failed!\n", ViPipe);

        stPubAttr.stWndRect.s32X = 0;
        stPubAttr.stWndRect.s32Y = 0;
        stPubAttr.stWndRect.u32Width  = pstChnCfg->u32Width;
        stPubAttr.stWndRect.u32Height = pstChnCfg->u32Height;
        stPubAttr.stSnsSize.u32Width  = pstChnCfg->u32Width;
        stPubAttr.stSnsSize.u32Height = pstChnCfg->u32Height;
        stPubAttr.f32FrameRate        = pstChnCfg->f32Fps;
        stPubAttr.enWDRMode           = pstSnsCfg->enWDRMode;
        s32Ret = CVI_ISP_SetPubAttr(ViPipe, &stPubAttr);
        APP_IPCAM_CHECK_RET(s32Ret, "SetPubAttr fail, ViPipe[%d]\n", ViPipe);


        memset(&stsCfg, 0, sizeof(ISP_STATISTICS_CFG_S));
        s32Ret = CVI_ISP_GetStatisticsConfig(ViPipe, &stsCfg);
        APP_IPCAM_CHECK_RET(s32Ret, "ISP Get Statistic fail, ViPipe[%d]\n", ViPipe);
        stsCfg.stAECfg.stCrop[0].bEnable = 0;
        stsCfg.stAECfg.stCrop[0].u16X = 0;
        stsCfg.stAECfg.stCrop[0].u16Y = 0;
        stsCfg.stAECfg.stCrop[0].u16W = stPubAttr.stWndRect.u32Width;
        stsCfg.stAECfg.stCrop[0].u16H = stPubAttr.stWndRect.u32Height;
        memset(stsCfg.stAECfg.au8Weight, 1,
                AE_WEIGHT_ZONE_ROW * AE_WEIGHT_ZONE_COLUMN * sizeof(CVI_U8));

        // stsCfg.stAECfg.stCrop[1].bEnable = 0;
        // stsCfg.stAECfg.stCrop[1].u16X = 0;
        // stsCfg.stAECfg.stCrop[1].u16Y = 0;
        // stsCfg.stAECfg.stCrop[1].u16W = stPubAttr.stWndRect.u32Width;
        // stsCfg.stAECfg.stCrop[1].u16H = stPubAttr.stWndRect.u32Height;

        stsCfg.stWBCfg.u16ZoneRow = AWB_ZONE_ORIG_ROW;
        stsCfg.stWBCfg.u16ZoneCol = AWB_ZONE_ORIG_COLUMN;
        stsCfg.stWBCfg.stCrop.u16X = 0;
        stsCfg.stWBCfg.stCrop.u16Y = 0;
        stsCfg.stWBCfg.stCrop.u16W = stPubAttr.stWndRect.u32Width;
        stsCfg.stWBCfg.stCrop.u16H = stPubAttr.stWndRect.u32Height;
        stsCfg.stWBCfg.u16BlackLevel = 0;
        stsCfg.stWBCfg.u16WhiteLevel = 4095;
        stsCfg.stFocusCfg.stConfig.bEnable = 1;
        stsCfg.stFocusCfg.stConfig.u8HFltShift = 1;
        stsCfg.stFocusCfg.stConfig.s8HVFltLpCoeff[0] = 1;
        stsCfg.stFocusCfg.stConfig.s8HVFltLpCoeff[1] = 2;
        stsCfg.stFocusCfg.stConfig.s8HVFltLpCoeff[2] = 3;
        stsCfg.stFocusCfg.stConfig.s8HVFltLpCoeff[3] = 5;
        stsCfg.stFocusCfg.stConfig.s8HVFltLpCoeff[4] = 10;
        stsCfg.stFocusCfg.stConfig.stRawCfg.PreGammaEn = 0;
        stsCfg.stFocusCfg.stConfig.stPreFltCfg.PreFltEn = 1;
        stsCfg.stFocusCfg.stConfig.u16Hwnd = 17;
        stsCfg.stFocusCfg.stConfig.u16Vwnd = 15;
        stsCfg.stFocusCfg.stConfig.stCrop.bEnable = 0;
        // AF offset and size has some limitation.
        stsCfg.stFocusCfg.stConfig.stCrop.u16X = AF_XOFFSET_MIN;
        stsCfg.stFocusCfg.stConfig.stCrop.u16Y = AF_YOFFSET_MIN;
        stsCfg.stFocusCfg.stConfig.stCrop.u16W = stPubAttr.stWndRect.u32Width - AF_XOFFSET_MIN * 2;
        stsCfg.stFocusCfg.stConfig.stCrop.u16H = stPubAttr.stWndRect.u32Height - AF_YOFFSET_MIN * 2;
        //Horizontal HP0
        stsCfg.stFocusCfg.stHParam_FIR0.s8HFltHpCoeff[0] = 0;
        stsCfg.stFocusCfg.stHParam_FIR0.s8HFltHpCoeff[1] = 0;
        stsCfg.stFocusCfg.stHParam_FIR0.s8HFltHpCoeff[2] = 13;
        stsCfg.stFocusCfg.stHParam_FIR0.s8HFltHpCoeff[3] = 24;
        stsCfg.stFocusCfg.stHParam_FIR0.s8HFltHpCoeff[4] = 0;
        //Horizontal HP1
        stsCfg.stFocusCfg.stHParam_FIR1.s8HFltHpCoeff[0] = 1;
        stsCfg.stFocusCfg.stHParam_FIR1.s8HFltHpCoeff[1] = 2;
        stsCfg.stFocusCfg.stHParam_FIR1.s8HFltHpCoeff[2] = 4;
        stsCfg.stFocusCfg.stHParam_FIR1.s8HFltHpCoeff[3] = 8;
        stsCfg.stFocusCfg.stHParam_FIR1.s8HFltHpCoeff[4] = 0;
        //Vertical HP
        stsCfg.stFocusCfg.stVParam_FIR.s8VFltHpCoeff[0] = 13;
        stsCfg.stFocusCfg.stVParam_FIR.s8VFltHpCoeff[1] = 24;
        stsCfg.stFocusCfg.stVParam_FIR.s8VFltHpCoeff[2] = 0;

        stsCfg.unKey.bit1FEAeGloStat = 1;
        stsCfg.unKey.bit1FEAeLocStat = 1;
        stsCfg.unKey.bit1AwbStat1 = 1;
        stsCfg.unKey.bit1AwbStat2 = 1;
        stsCfg.unKey.bit1FEAfStat = 1;

        //LDG
        stsCfg.stFocusCfg.stConfig.u8ThLow = 0;
        stsCfg.stFocusCfg.stConfig.u8ThHigh = 255;
        stsCfg.stFocusCfg.stConfig.u8GainLow = 30;
        stsCfg.stFocusCfg.stConfig.u8GainHigh = 20;
        stsCfg.stFocusCfg.stConfig.u8SlopLow = 8;
        stsCfg.stFocusCfg.stConfig.u8SlopHigh = 15;

        s32Ret = CVI_ISP_SetStatisticsConfig(ViPipe, &stsCfg);
        APP_IPCAM_CHECK_RET(s32Ret, "ISP Set Statistic fail, ViPipe[%d]\n", ViPipe);

        s32Ret = CVI_ISP_Init(ViPipe);
        APP_IPCAM_CHECK_RET(s32Ret, "ISP Init fail, ViPipe[%d]\n", ViPipe);

        s32Ret = app_ipcam_PQBin_Load(PQ_BIN_SDR);
        if (s32Ret != CVI_SUCCESS) {
            APP_PROF_LOG_PRINT(LEVEL_WARN, "load %s failed with %#x!\n", PQ_BIN_SDR, s32Ret);
        }
    }

    return CVI_SUCCESS;
}

int app_ipcam_Vi_Isp_DeInit(void)
{
    CVI_S32 s32Ret;
    VI_PIPE              ViPipe;
    ALG_LIB_S            ae_lib;
    ALG_LIB_S            awb_lib;

    ISP_SNS_OBJ_S *pfnSnsObj = CVI_NULL;

    for (CVI_U32 i = 0; i < g_pstViCtx->u32WorkSnsCnt; i++) {

        APP_PARAM_SNS_CFG_T *pstSnsCfg = &g_pstViCtx->astSensorCfg[i];
        APP_PARAM_CHN_CFG_T *pstChnCfg = &g_pstViCtx->astChnInfo[i];
        ViPipe = pstChnCfg->s32ChnId;

        pfnSnsObj = app_ipcam_SnsObj_Get(pstSnsCfg->enSnsType);
        if (pfnSnsObj == CVI_NULL) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR,"sensor obj(%d) is null\n", ViPipe);
            return CVI_FAILURE;
        }

        ae_lib.s32Id = ViPipe;
        awb_lib.s32Id = ViPipe;

        strcpy(ae_lib.acLibName, CVI_AE_LIB_NAME);//, sizeof(CVI_AE_LIB_NAME));
        strcpy(awb_lib.acLibName, CVI_AWB_LIB_NAME);//, sizeof(CVI_AWB_LIB_NAME));

        s32Ret = pfnSnsObj->pfnUnRegisterCallback(ViPipe, &ae_lib, &awb_lib);
        APP_IPCAM_CHECK_RET(s32Ret, "pfnUnRegisterCallback(%d) fail\n", ViPipe);

        s32Ret = CVI_AE_UnRegister(ViPipe, &ae_lib);
        APP_IPCAM_CHECK_RET(s32Ret, "CVI_AE_UnRegister(%d) fail\n", ViPipe);

        s32Ret = CVI_AWB_UnRegister(ViPipe, &awb_lib);
        APP_IPCAM_CHECK_RET(s32Ret, "CVI_AWB_UnRegister(%d) fail\n", ViPipe);

    }
    return CVI_SUCCESS;

}

static void callback_FPS(int fps)
{
    static CVI_FLOAT uMaxFPS[VI_MAX_DEV_NUM] = {0};
    CVI_U32 i;

    for (i = 0; i < VI_MAX_DEV_NUM && g_IspPid[i]; i++) {
        ISP_PUB_ATTR_S pubAttr = {0};

        CVI_ISP_GetPubAttr(i, &pubAttr);
        if (uMaxFPS[i] == 0) {
            uMaxFPS[i] = pubAttr.f32FrameRate;
        }
        if (fps == 0) {
            pubAttr.f32FrameRate = uMaxFPS[i];
        } else {
            pubAttr.f32FrameRate = (CVI_FLOAT) fps;
        }
        CVI_ISP_SetPubAttr(i, &pubAttr);
    }
}

void *ISP_Thread(void *arg)
{
    CVI_S32 s32Ret;
    VI_PIPE ViPipe = *(VI_PIPE *)arg;
    char szThreadName[20];

    snprintf(szThreadName, sizeof(szThreadName), "ISP%d_RUN", ViPipe);
    prctl(PR_SET_NAME, szThreadName, 0, 0, 0);

    // if (ViPipe > 0) {
    //     APP_PROF_LOG_PRINT(LEVEL_ERROR,"ISP Dev %d return\n", ViPipe);
    //     return CVI_NULL;
    // }

    CVI_SYS_RegisterThermalCallback(callback_FPS);

    APP_PROF_LOG_PRINT(LEVEL_INFO, "ISP Dev %d running!\n", ViPipe);
    s32Ret = CVI_ISP_Run(ViPipe);
    if (s32Ret != 0) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR,"CVI_ISP_Run(%d) failed with %#x!\n", ViPipe, s32Ret);
    }

    return CVI_NULL;
}

int app_ipcam_Vi_Isp_Start(void)
{
    CVI_S32 s32Ret;
    struct sched_param param;
    pthread_attr_t attr;

    VI_PIPE ViPipe;

    for (CVI_U32 i = 0; i < g_pstViCtx->u32WorkSnsCnt; i++) {

        APP_PARAM_SNS_CFG_T *pstSnsCfg = &g_pstViCtx->astSensorCfg[i];
        APP_PARAM_CHN_CFG_T *pstChnCfg = &g_pstViCtx->astChnInfo[i];
        ViPipe = pstChnCfg->s32ChnId;

        param.sched_priority = 80;
        pthread_attr_init(&attr);
        pthread_attr_setschedpolicy(&attr, SCHED_RR);
        pthread_attr_setschedparam(&attr, &param);
        pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
        s32Ret = pthread_create(&g_IspPid[ViPipe], &attr, ISP_Thread, (void *)&pstSnsCfg->s32SnsId);
        APP_IPCAM_CHECK_RET(s32Ret, "create isp running thread(%d) fail\n", ViPipe);
    }

    VI_DEV_ATTR_S pstDevAttr;
    for (CVI_U32 i = 0; i < g_pstViCtx->u32WorkSnsCnt; i++) {
        APP_PARAM_SNS_CFG_T *pstSnsCfg = &g_pstViCtx->astSensorCfg[i];
        APP_PARAM_CHN_CFG_T *pstChnCfg = &g_pstViCtx->astChnInfo[i];
        ViPipe = pstChnCfg->s32ChnId;
        CVI_VI_GetDevAttr(ViPipe, &pstDevAttr);
        s32Ret = CVI_BIN_SetBinName(pstDevAttr.stWDRAttr.enWDRMode, PQ_BIN_SDR);
        if (s32Ret != CVI_SUCCESS) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_BIN_SetBinName %s failed with %#x!\n", PQ_BIN_SDR, s32Ret);
            return s32Ret;
        }

        s32Ret = app_ipcam_Vi_framerate_Set(ViPipe, pstSnsCfg->s32Framerate);
        if (s32Ret != CVI_SUCCESS) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "app_ipcam_Vi_framerate_Set failed with %#x!\n", s32Ret);
            return s32Ret;
        }
    }

    s32Ret = app_ipcam_ISP_ProcInfo_Open(ISP_PROC_LOG_LEVEL_NONE);
    if (s32Ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "app_ipcam_ISP_ProcInfo_Open failed with %#x!\n", s32Ret);
        return s32Ret;
    }

    bAfFilterEnable = g_pstViCtx->astIspCfg[0].bAfFliter;
    if (bAfFilterEnable) {
        s32Ret = app_ipcam_Isp_AfFilter_Start();
        if(s32Ret != CVI_SUCCESS) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "app_ipcam_Isp_AfFilter_Start failed with %#x\n", s32Ret);
            return s32Ret;
        }
    }

    #ifdef SUPPORT_ISP_PQTOOL
    app_ipcam_Ispd_Load();
    #ifndef ARCH_CV183X
    app_ipcam_RawDump_Load();
    #endif
    #endif

    return CVI_SUCCESS;
}


int app_ipcam_Vi_Isp_Stop(void)
{
    CVI_S32 s32Ret;
    VI_PIPE ViPipe;

    for (CVI_U32 i = 0; i < g_pstViCtx->u32WorkSnsCnt; i++) {
        APP_PARAM_CHN_CFG_T *pstChnCfg = &g_pstViCtx->astChnInfo[i];
        ViPipe = pstChnCfg->s32ChnId;

        #ifdef SUPPORT_ISP_PQTOOL
        app_ipcam_Ispd_Unload();
        #ifndef ARCH_CV183X
        app_ipcam_RawDump_Unload();
        #endif
        #endif

        if (g_IspPid[ViPipe]) {
            s32Ret = CVI_ISP_Exit(ViPipe);
            if (s32Ret != CVI_SUCCESS) {
                 APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_ISP_Exit fail with %#x!\n", s32Ret);
                return CVI_FAILURE;
            }
            pthread_join(g_IspPid[ViPipe], NULL);
            g_IspPid[ViPipe] = 0;
            // SAMPLE_COMM_ISP_Sensor_UnRegiter_callback(ViPipe);
            // SAMPLE_COMM_ISP_Aelib_UnCallback(ViPipe);
            // SAMPLE_COMM_ISP_Awblib_UnCallback(ViPipe);
            // #if ENABLE_AF_LIB
            // SAMPLE_COMM_ISP_Aflib_UnCallback(ViPipe);
            // #endif
        }

    }

    return CVI_SUCCESS;
}

int app_ipcam_Vi_Chn_Start(void)
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

int app_ipcam_Vi_Chn_Stop(void)
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

