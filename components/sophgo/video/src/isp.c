#include <stdlib.h>
#include <sys/prctl.h>
#include <unistd.h>

#include <linux/cvi_comm_sys.h>
#include <cvi_ae.h>
#include <cvi_awb.h>
#include <cvi_bin.h>

#include "app_ipcam_paramparse.h"

/**************************************************************************
 *                              M A C R O S                               *
 **************************************************************************/
#ifdef SUPPORT_ISP_PQTOOL
#include <dlfcn.h>
#include "cvi_ispd2.h"

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
static pthread_t g_IspPid[VI_MAX_DEV_NUM];

#ifdef SUPPORT_ISP_PQTOOL
static CVI_BOOL bISPDaemon = CVI_FALSE;
//static CVI_VOID *pISPDHandle = NULL;
#ifndef ARCH_CV183X
static CVI_BOOL bRawDump = CVI_FALSE;
static CVI_VOID *pRawDumpHandle = NULL;
#endif
#endif

// static APP_PARAM_VI_PM_DATA_S ViPmData[VI_MAX_DEV_NUM] = { 0 };

/**************************************************************************
 *                 E X T E R N A L    R E F E R E N C E S                 *
 **************************************************************************/

/**************************************************************************
 *               F U N C T I O N    D E C L A R A T I O N S               *
 **************************************************************************/
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

static int app_ipcam_Vi_framerate_Set(VI_PIPE ViPipe, CVI_S32 framerate)
{
    ISP_PUB_ATTR_S pubAttr = {0};

    CVI_ISP_GetPubAttr(ViPipe, &pubAttr);

    pubAttr.f32FrameRate = (CVI_FLOAT)framerate;

    APP_CHK_RET(CVI_ISP_SetPubAttr(ViPipe, &pubAttr), "set vi framerate");

    return CVI_SUCCESS;
}

static int app_ipcam_PQBin_Load(const CVI_CHAR *pBinPath)
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

#if 0
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
#endif

int app_ipcam_Vi_Isp_Init(void)
{
    CVI_S32 s32Ret;
    VI_PIPE              ViPipe;
    ISP_PUB_ATTR_S       stPubAttr;
    ISP_STATISTICS_CFG_S stsCfg;
    ISP_BIND_ATTR_S      stBindAttr;
    ALG_LIB_S            stAeLib;
    ALG_LIB_S            stAwbLib;

    APP_PARAM_VI_CTX_S *g_pstViCtx = app_ipcam_Vi_Param_Get();

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
    APP_PARAM_VI_CTX_S *g_pstViCtx = app_ipcam_Vi_Param_Get();

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

static void *ISP_Thread(void *arg)
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
    APP_PARAM_VI_CTX_S *g_pstViCtx = app_ipcam_Vi_Param_Get();

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

#if 0
    bAfFilterEnable = g_pstViCtx->astIspCfg[0].bAfFliter;
    if (bAfFilterEnable) {
        s32Ret = app_ipcam_Isp_AfFilter_Start();
        if(s32Ret != CVI_SUCCESS) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "app_ipcam_Isp_AfFilter_Start failed with %#x\n", s32Ret);
            return s32Ret;
        }
    }
#endif

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
    APP_PARAM_VI_CTX_S *g_pstViCtx = app_ipcam_Vi_Param_Get();

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
