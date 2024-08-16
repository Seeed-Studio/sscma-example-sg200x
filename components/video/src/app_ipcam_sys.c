#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include "cvi_type.h"
#include "cvi_vb.h"
#include "cvi_buffer.h"
#include "app_ipcam_sys.h"
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
APP_PARAM_SYS_CFG_S g_stSysAttrCfg, *g_pstSysAttrCfg = &g_stSysAttrCfg;

/**************************************************************************
 *                 E X T E R N A L    R E F E R E N C E S                 *
 **************************************************************************/

/**************************************************************************
 *               F U N C T I O N    D E C L A R A T I O N S               *
 **************************************************************************/
APP_PARAM_SYS_CFG_S *app_ipcam_Sys_Param_Get(void)
{
    return g_pstSysAttrCfg;
}

static int COMM_SYS_Init(VB_CONFIG_S *pstVbConfig)
{
    CVI_S32 s32Ret = CVI_FAILURE;

    CVI_SYS_Exit();
    CVI_VB_Exit();

    if (pstVbConfig == NULL) {
        APP_PROF_LOG_PRINT(LEVEL_INFO, "input parameter is null, it is invaild!\n");
        return APP_IPCAM_ERR_FAILURE;
    }

    s32Ret = CVI_VB_SetConfig(pstVbConfig);
    if (s32Ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_INFO, "CVI_VB_SetConf failed!\n");
        return s32Ret;
    }

    s32Ret = CVI_VB_Init();
    if (s32Ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_INFO, "CVI_VB_Init failed!\n");
        return s32Ret;
    }

    s32Ret = CVI_SYS_Init();
    if (s32Ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_INFO, "CVI_SYS_Init failed!\n");
        CVI_VB_Exit();
        return s32Ret;
    }

    #ifdef FAST_BOOT_ENABLE
    s32Ret = CVI_EFUSE_EnableFastBoot();
    if (s32Ret < 0) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_EFUSE_EnableFastBoot failed with %#x\n", s32Ret);
        return CVI_FAILURE;
    }

    s32Ret = CVI_EFUSE_IsFastBootEnabled();
    if (s32Ret < 0) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_EFUSE_IsFastBootEnabled failed with %#x\n", s32Ret);
        return CVI_FAILURE;
    }

    CVI_TRACE_LOG(CVI_DBG_INFO, "cv182x enable fastboot done\n");
    #endif

    return CVI_SUCCESS;
}

static uint32_t get_frame_size(
        uint32_t w, 
        uint32_t h, 
        PIXEL_FORMAT_E fmt,
        DATA_BITWIDTH_E enBitWidth,
        COMPRESS_MODE_E enCmpMode) 
{
    // try rotate and non-rotate, choose the larger one

    uint32_t size_w_h = COMMON_GetPicBufferSize(w, h, fmt,
                            enBitWidth, enCmpMode, DEFAULT_ALIGN);

    #if 0
    uint32_t size_h_w = COMMON_GetPicBufferSize(h, w, fmt,
        enBitWidth, enCmpMode, DEFAULT_ALIGN);
    return (size_w_h > size_h_w) ? size_w_h : size_h_w;
    #else
    return size_w_h;
    #endif
}

int app_ipcam_Sys_DeInit(void)
{
    APP_CHK_RET(CVI_VB_Exit()," Vb Exit");
    APP_CHK_RET(CVI_SYS_Exit()," Systerm Exit");

    return CVI_SUCCESS;
}

/// pass resolution and blkcnt directly for now
/// and assuming yuv420 for now
/// TODO: refactor to attribute struct
int app_ipcam_Sys_Init(void)
{
    APP_PROF_LOG_PRINT(LEVEL_INFO, "system init ------------------> start \n");
    
    int ret = CVI_SUCCESS;

    ret = CVI_VI_SetDevNum(app_ipcam_Vi_Param_Get()->u32WorkSnsCnt);
    if (ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_VI_SetDevNum_%d failed with %#x\n", app_ipcam_Vi_Param_Get()->u32WorkSnsCnt, ret);
        goto error;
    }
    APP_PARAM_SYS_CFG_S *pattr = app_ipcam_Sys_Param_Get();

    //struct sigaction sa;
    //memset(&sa, 0, sizeof(struct sigaction));
    //sigemptyset(&sa.sa_mask);
    //sa.sa_sigaction = _SYS_HandleSig;
    //sa.sa_flags = SA_SIGINFO|SA_RESETHAND;    // Reset signal handler to system default after signal triggered
    //sigaction(SIGINT, &sa, NULL);
    //sigaction(SIGTERM, &sa, NULL);

    VB_CONFIG_S      stVbConf;
    memset(&stVbConf, 0, sizeof(VB_CONFIG_S));

    for (unsigned i = 0; i < pattr->vb_pool_num; i++) {
        uint32_t blk_size = get_frame_size(
                    pattr->vb_pool[i].width,
                    pattr->vb_pool[i].height,
                    pattr->vb_pool[i].fmt,
                    pattr->vb_pool[i].enBitWidth,
                    pattr->vb_pool[i].enCmpMode);

        uint32_t blk_num = pattr->vb_pool[i].vb_blk_num;

        stVbConf.astCommPool[i].u32BlkSize    = blk_size;
        stVbConf.astCommPool[i].u32BlkCnt     = blk_num;
        stVbConf.astCommPool[i].enRemapMode   = VB_REMAP_MODE_CACHED;

        stVbConf.u32MaxPoolCnt++;
        APP_PROF_LOG_PRINT(LEVEL_INFO, "VB pool[%d] BlkSize %d BlkCnt %d\n", i, blk_size, blk_num);
    }

    CVI_S32 rc = COMM_SYS_Init(&stVbConf);
    if (rc != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_INFO, "COMM_SYS_Init fail, rc = %#x\n", rc);
        ret = APP_IPCAM_ERR_FAILURE;
        goto error;
    }

    rc = CVI_SYS_SetVIVPSSMode(&pattr->stVIVPSSMode);
    if (rc != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_INFO, "CVI_SYS_SetVIVPSSMode failed with %#x\n", rc);
        ret = APP_IPCAM_ERR_FAILURE;
        goto error;
    }

    rc = CVI_SYS_SetVPSSModeEx(&pattr->stVPSSMode);
    if (rc != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_INFO, "CVI_SYS_SetVPSSModeEx failed with %#x\n", rc);
        ret = APP_IPCAM_ERR_FAILURE;
        goto error;
    }

    APP_PROF_LOG_PRINT(LEVEL_INFO, "system init ------------------> done \n");
    return ret;

error:
    app_ipcam_Sys_DeInit();
    return ret;
}
