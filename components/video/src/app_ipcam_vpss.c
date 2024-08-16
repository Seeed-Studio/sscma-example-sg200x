#include "app_ipcam_vpss.h"
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
APP_PARAM_VPSS_CFG_T g_stVpssCfg;
APP_PARAM_VPSS_CFG_T *g_pstVpssCfg = &g_stVpssCfg;

/**************************************************************************
 *                 E X T E R N A L    R E F E R E N C E S                 *
 **************************************************************************/

/**************************************************************************
 *               F U N C T I O N    D E C L A R A T I O N S               *
 **************************************************************************/

APP_PARAM_VPSS_CFG_T *app_ipcam_Vpss_Param_Get(void)
{
    return g_pstVpssCfg;
}

int app_ipcam_Vpss_Destroy(VPSS_GRP VpssGrp)
{
    CVI_S32 s32Ret = CVI_SUCCESS;

    APP_VPSS_GRP_CFG_T *pstVpssGrpCfg = &g_pstVpssCfg->astVpssGrpCfg[VpssGrp];
    if (!pstVpssGrpCfg->bCreate) {
        APP_PROF_LOG_PRINT(LEVEL_WARN, "VpssGrp(%d) not create yet!\n", VpssGrp);
        return CVI_SUCCESS;
    }

    for (VPSS_CHN VpssChn = 0; VpssChn < VPSS_MAX_PHY_CHN_NUM; VpssChn++) {
        if (pstVpssGrpCfg->abChnCreate[VpssChn]) {
            s32Ret = CVI_VPSS_DisableChn(VpssGrp, VpssChn);
            if (s32Ret != CVI_SUCCESS) {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_VPSS_DisableChn failed with %#x!\n", s32Ret);
                return CVI_FAILURE;
            }
            pstVpssGrpCfg->abChnCreate[VpssChn] = CVI_FALSE;
        }
    }

    s32Ret = CVI_VPSS_StopGrp(VpssGrp);
    if (s32Ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_VPSS_StopGrp failed with %#x!\n", s32Ret);
        return CVI_FAILURE;
    }

    s32Ret = CVI_VPSS_DestroyGrp(VpssGrp);
    if (s32Ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_VPSS_DestroyGrp failed with %#x!\n", s32Ret);
        return CVI_FAILURE;
    }

    pstVpssGrpCfg->bCreate = CVI_FALSE;

    return CVI_SUCCESS;
}

int app_ipcam_Vpss_Create(VPSS_GRP VpssGrp)
{
    CVI_S32 s32Ret = CVI_SUCCESS;
    APP_PARAM_SYS_CFG_S *pstSysCfg = app_ipcam_Sys_Param_Get();
    CVI_BOOL bSBMEnable = pstSysCfg->bSBMEnable;

    APP_VPSS_GRP_CFG_T *pstVpssGrpCfg = &g_pstVpssCfg->astVpssGrpCfg[VpssGrp];
    if (!pstVpssGrpCfg->bEnable) {
        APP_PROF_LOG_PRINT(LEVEL_WARN, "VpssGrp(%d) not Enable!\n", VpssGrp);
        return CVI_SUCCESS;
    }

    if (pstVpssGrpCfg->bCreate) {
        APP_PROF_LOG_PRINT(LEVEL_WARN, "VpssGrp(%d) have been created!\n", VpssGrp);
        return CVI_SUCCESS;
    }

    APP_PROF_LOG_PRINT(LEVEL_INFO, "GrpID=%d isEnable=%d\n", pstVpssGrpCfg->VpssGrp, pstVpssGrpCfg->bEnable);

    if (CVI_VPSS_CreateGrp(pstVpssGrpCfg->VpssGrp, &pstVpssGrpCfg->stVpssGrpAttr) != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_VPSS_CreateGrp(grp:%d) failed with %d!\n", pstVpssGrpCfg->VpssGrp, s32Ret);
        goto VPSS_EXIT;
    }

    if (CVI_VPSS_ResetGrp(pstVpssGrpCfg->VpssGrp) != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_VPSS_ResetGrp(grp:%d) failed with %d!\n", pstVpssGrpCfg->VpssGrp, s32Ret);
        goto VPSS_EXIT;
    }

    // Vpss Group not support crop if online
    // APP_PARAM_SYS_CFG_S *pstSysCfg = app_ipcam_Sys_Param_Get();
    // VI_VPSS_MODE_E vi_vpss_mode = pstSysCfg->stVIVPSSMode.aenMode[0];
    // if (vi_vpss_mode == VI_OFFLINE_VPSS_OFFLINE || vi_vpss_mode == VI_ONLINE_VPSS_OFFLINE) {
    //     if (CVI_VPSS_SetGrpCrop(pstVpssGrpCfg->VpssGrp, &pstVpssGrpCfg->stVpssGrpCropInfo) != CVI_SUCCESS) {
    //         APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_VPSS_SetGrpCrop failed with %d\n", s32Ret);
    //         goto VPSS_EXIT;
    //     }
    // }

    for (VPSS_CHN VpssChn = 0; VpssChn < VPSS_MAX_PHY_CHN_NUM; VpssChn++) {
        APP_PROF_LOG_PRINT(LEVEL_INFO, "\tChnID=%d isEnable=%d\n", VpssChn, pstVpssGrpCfg->abChnEnable[VpssChn]);
        if (pstVpssGrpCfg->abChnEnable[VpssChn]) {

            if (CVI_VPSS_SetChnAttr(pstVpssGrpCfg->VpssGrp, VpssChn, &pstVpssGrpCfg->astVpssChnAttr[VpssChn]) != CVI_SUCCESS) {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_VPSS_SetChnAttr(%d) failed with %d\n", VpssChn, s32Ret);
                goto VPSS_EXIT;
            }

            if (CVI_VPSS_SetChnCrop(pstVpssGrpCfg->VpssGrp, VpssChn, &pstVpssGrpCfg->stVpssChnCropInfo[VpssChn]) != CVI_SUCCESS) {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_VPSS_SetChnCrop(%d) failed with %d\n", VpssChn, s32Ret);
                goto VPSS_EXIT;
            }

            if ((bSBMEnable) && (VpssGrp == 0) && (VpssChn == 0)) {
                APP_PROF_LOG_PRINT(LEVEL_INFO, "CVI_VPSS_SetChnBufWrapAttr grp(%d) chn(%d) \n", VpssGrp, VpssChn);
                VPSS_CHN_BUF_WRAP_S stVpssChnBufWrap = {0};
                stVpssChnBufWrap.bEnable = CVI_TRUE;
                stVpssChnBufWrap.u32BufLine = 64;
                stVpssChnBufWrap.u32WrapBufferSize = 5;
                s32Ret = CVI_VPSS_SetChnBufWrapAttr(0, 0, &stVpssChnBufWrap);
                if (s32Ret != CVI_SUCCESS) {
                    APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_VPSS_SetChnBufWrapAttr failed with %d\n", s32Ret);
                    goto VPSS_EXIT;
                }
            }

            if (CVI_VPSS_EnableChn(pstVpssGrpCfg->VpssGrp, VpssChn) != CVI_SUCCESS) {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_VPSS_EnableChn(%d) failed with %d\n", VpssChn, s32Ret);
                goto VPSS_EXIT;
            }

            pstVpssGrpCfg->abChnCreate[VpssChn] = CVI_TRUE;

            if (pstVpssGrpCfg->aAttachEn[VpssChn]) {
                if (CVI_VPSS_AttachVbPool(pstVpssGrpCfg->VpssGrp, VpssChn, pstVpssGrpCfg->aAttachPool[VpssChn]) != CVI_SUCCESS) {
                    APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_VPSS_AttachVbPool failed with %d\n", s32Ret);
                    goto VPSS_EXIT;
                }
            }
        }
    }

    s32Ret = CVI_VPSS_StartGrp(pstVpssGrpCfg->VpssGrp);
    if (s32Ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "start vpss group failed. s32Ret: 0x%x !\n", s32Ret);
        return s32Ret;
    }

    pstVpssGrpCfg->bCreate = CVI_TRUE;

    return CVI_SUCCESS;

VPSS_EXIT:
    app_ipcam_Vpss_DeInit();
    app_ipcam_Vi_DeInit();
    app_ipcam_Sys_DeInit();

    return s32Ret;

}

int app_ipcam_Vpss_Unbind(VPSS_GRP VpssGrp)
{
    CVI_S32 s32Ret = CVI_SUCCESS;

    APP_VPSS_GRP_CFG_T *pstVpssGrpCfg = &g_pstVpssCfg->astVpssGrpCfg[VpssGrp];
    if (!pstVpssGrpCfg->bEnable) {
        APP_PROF_LOG_PRINT(LEVEL_WARN, "VpssGrp(%d) not Enable!\n", VpssGrp);
        return CVI_SUCCESS;
    }

    APP_PROF_LOG_PRINT(LEVEL_INFO, "VpssGrp=%d bindMode:%d\n", VpssGrp, pstVpssGrpCfg->bBindMode);
    
    if (pstVpssGrpCfg->bBindMode) {
        s32Ret = CVI_SYS_UnBind(&pstVpssGrpCfg->astChn[0], &pstVpssGrpCfg->astChn[1]);
        if (s32Ret != CVI_SUCCESS) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_SYS_UnBind failed with %#x\n", s32Ret);
            return s32Ret;
        }
    }

    return CVI_SUCCESS;
}

int app_ipcam_Vpss_Bind(VPSS_GRP VpssGrp)
{
    CVI_S32 s32Ret = CVI_SUCCESS;

    APP_VPSS_GRP_CFG_T *pstVpssGrpCfg = &g_pstVpssCfg->astVpssGrpCfg[VpssGrp];
    if (!pstVpssGrpCfg->bEnable) {
        APP_PROF_LOG_PRINT(LEVEL_WARN, "VpssGrp(%d) not Enable!\n", VpssGrp);
        return CVI_SUCCESS;
    }

    APP_PROF_LOG_PRINT(LEVEL_INFO, "GrpID=%d isEnable=%d\n", pstVpssGrpCfg->VpssGrp, pstVpssGrpCfg->bEnable);

    if (pstVpssGrpCfg->bBindMode) {
        s32Ret = CVI_SYS_Bind(&pstVpssGrpCfg->astChn[0], &pstVpssGrpCfg->astChn[1]);
        if (s32Ret != CVI_SUCCESS) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_SYS_Bind failed with %#x\n", s32Ret);
            return s32Ret;
        }
    }

    return CVI_SUCCESS;
}

int app_ipcam_Vpss_DeInit(void)
{
    CVI_S32 s32Ret = CVI_SUCCESS;

    for (CVI_U32 VpssGrp = 0; VpssGrp < g_pstVpssCfg->u32GrpCnt; VpssGrp++) {
        s32Ret = app_ipcam_Vpss_Destroy(VpssGrp);
        if (s32Ret != CVI_SUCCESS) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "Vpss grp(%d) Destroy failed with 0x%x!\n", VpssGrp, s32Ret);
            return s32Ret;
        }

        s32Ret = app_ipcam_Vpss_Unbind(VpssGrp);
        if (s32Ret != CVI_SUCCESS) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "Vpss grp(%d) Unbind failed with 0x%x!\n", VpssGrp, s32Ret);
            return s32Ret;
        }
    }

    return CVI_SUCCESS;
}

int app_ipcam_Vpss_Init(void)
{
    CVI_S32 s32Ret = CVI_SUCCESS;

    APP_PROF_LOG_PRINT(LEVEL_INFO, "vpss init ------------------> start \n");

    for (CVI_U32 VpssGrp = 0; VpssGrp < g_pstVpssCfg->u32GrpCnt; VpssGrp++) {
        s32Ret = app_ipcam_Vpss_Create(VpssGrp);
        if (s32Ret != CVI_SUCCESS) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "Vpss grp(%d) Create failed with 0x%x!\n", VpssGrp, s32Ret);
            return s32Ret;
        }
        s32Ret = app_ipcam_Vpss_Bind(VpssGrp);
        if (s32Ret != CVI_SUCCESS) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "Vpss grp(%d) Bind failed with 0x%x!\n", VpssGrp, s32Ret);
            return s32Ret;
        }
    }

    APP_PROF_LOG_PRINT(LEVEL_INFO, "vpss init ------------------> end \n");

    return CVI_SUCCESS;
}

/*****************************************************************
 *  The following API for command test used             Front
 * **************************************************************/

int app_ipcam_CmdTask_Rotate_Switch(CVI_MQ_MSG_t *msg, CVI_VOID *userdate)
{
    CVI_CHAR param[512] = {0};
    snprintf(param, sizeof(param), "%s", msg->payload);
    APP_PROF_LOG_PRINT(LEVEL_INFO, "%s param:%s arg2=%d\n", __FUNCTION__, param, msg->arg2);

    CVI_CHAR *temp = strtok(param, ":");
    while(NULL != temp) {
        APP_PROF_LOG_PRINT(LEVEL_INFO, "%s switch case -> %c \n", __FUNCTION__, *temp);
        switch (*temp) {
            case 's': 
                {
                    temp = strtok(NULL, "/");
                    CVI_BOOL bEnable = (CVI_BOOL)atoi(temp);
                    if (bEnable) {
                        CVI_VPSS_SetChnRotation(0, 0, ROTATION_180);
                    } else {
                        CVI_VPSS_SetChnRotation(0, 0, ROTATION_180);
                    }
                }
                break;
            default:
                return 0;
                break;
        }
        
        temp = strtok(NULL, ":");
    }

    return CVI_SUCCESS;
}


/*****************************************************************
 *  The above API for command test used                 End
 * **************************************************************/
