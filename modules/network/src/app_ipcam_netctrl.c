#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/route.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <semaphore.h>
#include <sys/time.h>
#include "cvi_isp.h"
#include "cvi_vpss.h"
#include "cvi_comm_isp.h"
#include "cvi_ae.h"
#include "cJSON.h"
#include "app_ipcam_sys.h"
#include "app_ipcam_netctrl.h"
#include "app_ipcam_net.h"
#include "app_ipcam_websocket.h"
#include "app_ipcam_comm.h"
#include "app_ipcam_vi.h"
#include "app_ipcam_vpss.h"
#include "app_ipcam_venc.h"
#include "app_ipcam_ircut.h"
#include "app_ipcam_rtsp.h"
#include "app_ipcam_ota.h"
#ifdef AI_SUPPORT
#include "app_ipcam_ai.h"
#endif
#ifdef OSD_SUPPORT
#include "app_ipcam_osd.h"
#endif
#ifdef AUDIO_SUPPORT
#include "app_ipcam_audio.h"
#endif
#ifdef RECORD_SUPPORT
#include "app_ipcam_record.h"
#endif

/**************************************************************************
 *                              M A C R O S                               *
 **************************************************************************/
#define NET_VALUE_LEN_MAX   (64)

#define APP_RESOLUTION_INDEX_GET(CHN, W, H, IDX) do {                                                               \
    if (CHN == 0) { /* main stream size list */                                                                     \
        if ((W == APP_RESOLUTION_1440P_W) && (H == APP_RESOLUTION_1440P_H)) {                                       \
            IDX = 0;                                                                                                \
        } else if ((W == APP_RESOLUTION_1080P_W) && (H == APP_RESOLUTION_1080P_H)) {                                \
            IDX = 1;                                                                                                \
        } else if ((W == APP_RESOLUTION_720P_W) && (H == APP_RESOLUTION_720P_H)) {                                  \
            IDX = 2;                                                                                                \
        } else {                                                                                                    \
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "RESOLUTION INDEX GET chn(%d) size(%dx%d) not match \n", CHN, W, H);    \
        }                                                                                                           \
    } else if (CHN == 1) { /* sub stream size list */                                                               \
        if ((W == APP_RESOLUTION_576P_W) && (H == APP_RESOLUTION_576P_H)) {                                         \
            IDX = 0;                                                                                                \
        } else if ((W == APP_RESOLUTION_360P_W) && (H == APP_RESOLUTION_360P_H)) {                                  \
            IDX = 1;                                                                                                \
        } else {                                                                                                    \
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "RESOLUTION INDEX GET chn(%d) size(%dx%d) not match \n", CHN, W, H);    \
        }                                                                                                           \
    } else if (CHN == 2) { /* sub capture size list */                                                              \
        if ((W == APP_RESOLUTION_1440P_W) && (H == APP_RESOLUTION_1440P_H)) {                                       \
            IDX = 0;                                                                                                \
        } else if ((W == APP_RESOLUTION_1080P_W) && (H == APP_RESOLUTION_1080P_H)) {                                \
            IDX = 1;                                                                                                \
        } else if ((W == APP_RESOLUTION_720P_W) && (H == APP_RESOLUTION_720P_H)) {                                  \
            IDX = 2;                                                                                                \
        } else if ((W == APP_RESOLUTION_576P_W) && (H == APP_RESOLUTION_576P_H)) {                                  \
            IDX = 3;                                                                                                \
        } else if ((W == APP_RESOLUTION_360P_W) && (H == APP_RESOLUTION_360P_H)) {                                  \
            IDX = 4;                                                                                                \
        } else {                                                                                                    \
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "RESOLUTION INDEX GET chn(%d) size(%dx%d) not match \n", CHN, W, H);    \
        }                                                                                                           \
    } else {                                                                                                        \
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "RESOLUTION INDEX GET chn(%d) not match \n", CHN);                          \
    }                                                                                                               \
} while (0)

#define APP_RESOLUTION_GET(CHN, IDX, W, H) do {                                                     \
    if (CHN == 0) {                                                                                 \
        if (IDX == 0) {                                                                             \
            W = APP_RESOLUTION_1440P_W;                                                             \
            H = APP_RESOLUTION_1440P_H;                                                             \
        } else if (IDX == 1) {                                                                      \
            W = APP_RESOLUTION_1080P_W;                                                             \
            H = APP_RESOLUTION_1080P_H;                                                             \
        } else if (IDX == 2) {                                                                      \
            W = APP_RESOLUTION_720P_W;                                                              \
            H = APP_RESOLUTION_720P_H;                                                              \
        } else {                                                                                    \
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "CHN (%d) RESOLUTION IDX (%d) not match \n", CHN, IDX); \
        }                                                                                           \
    } else if (CHN == 1) {                                                                          \
        if (IDX == 0) {                                                                             \
            W = APP_RESOLUTION_576P_W;                                                              \
            H = APP_RESOLUTION_576P_H;                                                              \
        } else if (IDX == 1) {                                                                      \
            W = APP_RESOLUTION_360P_W;                                                              \
            H = APP_RESOLUTION_360P_H;                                                              \
        } else {\
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "CHN (%d) RESOLUTION IDX (%d) not match \n", CHN, IDX); \
        }                                                                                           \
    } else if (CHN == 2) {                                                                          \
        if (IDX == 0) {                                                                             \
            W = APP_RESOLUTION_1440P_W;                                                             \
            H = APP_RESOLUTION_1440P_H;                                                             \
        } else if (IDX == 1) {                                                                      \
            W = APP_RESOLUTION_1080P_W;                                                             \
            H = APP_RESOLUTION_1080P_H;                                                             \
        } else if (IDX == 2) {                                                                      \
            W = APP_RESOLUTION_720P_W;                                                              \
            H = APP_RESOLUTION_720P_H;                                                              \
        } else if (IDX == 3) {                                                                      \
            W = APP_RESOLUTION_576P_W;                                                              \
            H = APP_RESOLUTION_576P_H;                                                              \
        } else if (IDX == 4) {                                                                      \
            W = APP_RESOLUTION_360P_W;                                                              \
            H = APP_RESOLUTION_360P_H;                                                              \
        } else {                                                                                    \
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "CHN (%d) RESOLUTION IDX (%d) not match \n", CHN, IDX); \
        }                                                                                           \
    } else {                                                                                        \
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "RESOLUTION INDEX (%d) not match \n", IDX);                 \
    }                                                                                               \
} while(0)

#define APP_PARAM_SAME_CHK(SRC, DST, FLAG) do {     \
    if (SRC != DST) {                               \
        FLAG = CVI_TRUE;                            \
        continue;                                   \
    } else if (SRC == DST) {                        \
        FLAG = CVI_FALSE;                           \
        continue;                                   \
    }                                               \
} while(0);

#define OSD_TIME_INDEX  0
#define OSD_TEXT1_INDEX 1
#define OSD_TEXT2_INDEX 1
#define OSD_TEXT3_INDEX 7


/**************************************************************************
 *                           C O N S T A N T S                            *
 **************************************************************************/
typedef enum APP_ROTATION_T {
    APP_ENUM_MIRROR,
    APP_ENUM_FLIP,
    APP_ENUM_180,
} APP_ROTATION_E;
/**************************************************************************
 *                 E X T E R N A L    R E F E R E N C E S                 *
 **************************************************************************/
extern struct lws *g_wsi;

/**************************************************************************
 *                         G L O B A L    D A T A                         *
 **************************************************************************/
static int s_noise2d = 50;
static int s_noise3d = 50;
static int s_compensationEnable = 0;
static int s_compensation = 50;
static int s_suppressionEnabled = 0;
static int s_suppression = 50;
static int s_whiteBalance = 0;
static int s_redGain = 50;
static int s_blueGain = 50;
static int s_defogEnabled = 0;
static int s_defog = 50;
static int s_shutterEnabled = 0;
static int s_shutter = 100;
static int s_distortionEnabled = 0;
static int s_distortion = 50;
static int s_frequency = 0;
static int s_antiflash = 0;
static int s_irCut = 0;
static int s_keepColor = 0;
static int s_ds = 0;

APP_VENC_ATTR_CHANGE_S stResetVencFlag[APP_VENC_CHN_NUM];
/**************************************************************************
 *               F U N C T I O N    D E C L A R A T I O N S               *
 **************************************************************************/
static int IcgiRegister(const char *cmd, const char *val, void *cb)
{
    printf("enter: %s ", __func__);
    
    int ret = 0;
    CGI_CMD_S stCgiCmd;

    if (cmd != NULL) {
        printf("%s %zu  ", cmd, strlen(cmd)+1);
        snprintf(stCgiCmd.cgi, strlen(cmd)+1, cmd);
    } else {
        printf("error, cmd is NULL\n");
        ret = -1;
        goto EXIT;
    }

    if (val != NULL) {
        printf("%s %zu", val, strlen(val)+1);
        snprintf(stCgiCmd.cmd, strlen(val)+1, val);
    }
    printf("\n");

    if (cb == NULL) {
        ret = -1;
        goto EXIT;
    }
    stCgiCmd.callback = (CGI_CALLBACK)cb;
    CVI_NET_RegisterCgiCmd(&stCgiCmd);

EXIT:
    return ret;
}

static int Hex2Dec(char c)
{
    if ('0' <= c && c <= '9') {
        return c - '0';
    } else if ('a' <= c && c <= 'f') {
        return c - 'a' + 10;
    } else if ('A' <= c && c <= 'F') {
        return c - 'A' + 10;
    } else {
        return -1;
    }
}

static int UrlDecode(const char url[], char *result)
{
    int i = 0;
    int len = strlen(url);
    int res_len = 0;
    for (i = 0; i < len; ++i) {
        char c = url[i];
        if (c != '%') {
            result[res_len++] = c;
        } else {
            char c1 = url[++i];
            char c0 = url[++i];
            int num = 0;
            num = Hex2Dec(c1) * 16 + Hex2Dec(c0);
            result[res_len++] = num;
        }
    }
    result[res_len] = '\0';
    return res_len;
}

static int NetString2Int(const char *val)
{
    char *pValue = NULL;
    char array[NET_VALUE_LEN_MAX] = {0};
    int value = -1;

    pValue = strrchr(val, '=');
    strncpy(array, pValue + 1, NET_VALUE_LEN_MAX - 1);
    value = atoi(array);
    printf("%s---int: %d\n", val, value);
    return value;
}

static void NetString2Str(const char *val, char *dstStr)
{
    char *pValue = NULL;
    pValue = strrchr(val, '=');
    strncpy(dstStr, pValue + 1, NET_VALUE_LEN_MAX - 1);
}

/*
*   preview page start
*/
static int app_ipcam_LocalIP_Get(const char* adapterName, char* ipAddr) 
{
    struct ifreq ifr;
    struct sockaddr addr;
    int skfd;
    const char* ifname = adapterName;

    skfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (skfd < 0) {
        printf("socket failed! strerror : %s\n", strerror(errno));
        return -1;
    }
    strcpy(ifr.ifr_name, ifname);
    ifr.ifr_addr.sa_family = AF_INET;
    memset(&addr, 0, sizeof(struct sockaddr));

    if (ioctl(skfd, SIOCGIFADDR, &ifr) == 0) {
        addr = ifr.ifr_addr;
    } else {
        printf("ioctl failed! strerror : %s\n", strerror(errno));
        close(skfd);
        return -1;
    }
    strcpy(ipAddr, inet_ntoa(((struct sockaddr_in*)&addr)->sin_addr));
    close(skfd);
    return 0;
}

static int GetWsAddrCallBack(void* param, const char* cmd, const char* val) 
{
    printf("enter: %s [%s|%s]\n", __func__, cmd, val);

    char ip[64] = "0.0.0.0";

    //FIXME: Only one websocket connection is supported.
    if (g_wsi == NULL) {
        if (app_ipcam_LocalIP_Get("eth0", ip) != 0) {
            if (app_ipcam_LocalIP_Get("wlan0", ip) != 0) {
                if (app_ipcam_LocalIP_Get("usb0", ip) != 0) {
                    printf("Error: [%s][%d] getlocalIP [eth0/wlan0/usb0] failed!\n",
                        __func__, __LINE__);
                }
            }
        }
        printf("getlocalIP [eth0/wlan0/usb0] %s\n", ip);
    }

    CVI_NET_AddCgiResponse(param, "ws://%s:8000", ip);

    return 0;
}

static int GetCurChnCallBack(void *param, const char *cmd, const char *val)
{
    printf("enter: %s [%s|%s]\n", __func__, cmd, val);
    int chn = 0;
    cJSON* cjsonCurChn = NULL;
    char* str = NULL;

    chn = app_ipcam_WebSocketChn_Get();

    cjsonCurChn = cJSON_CreateObject();
    cJSON_AddNumberToObject(cjsonCurChn, "current_chn", chn);
    str = cJSON_Print(cjsonCurChn);
    if (str) {
        CVI_NET_AddCgiResponse(param, "%s", str);
        cJSON_free(str);
    }

    cJSON_Delete(cjsonCurChn);
    return 0;
}

static int SetCurChnCallBack(void *param, const char *cmd, const char *val)
{
    int ret = -1;
    int value = NetString2Int(val);
    ret = app_ipcam_WebSocketChn_Set(value);

    CVI_NET_AddCgiResponse(param, "switch_stream: %d\n", ret);
    return 0;
}

static int TakePhotoCallBack(void *param, const char *cmd, const char *val)
{
    int ret = CVI_SUCCESS;

    app_ipcam_JpgCapFlag_Set(CVI_TRUE);

    CVI_NET_AddCgiResponse(param, "switch_stream: %d\n", ret);
    return 0;
}
/*
*   preview page end
*/

/*
*   image page start
*/
static int ImagePage_Get_Amp(PROC_AMP_E AmpType)
{
    int s32Ret = CVI_SUCCESS;
    int vpssGrp = 0;

    CVI_S32 value = 0;
    s32Ret = CVI_VPSS_GetGrpProcAmp(vpssGrp, AmpType, &value);
    if (s32Ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "get vpss grp amp info failed with %#x!\n", s32Ret);
        return s32Ret;
    }

    return value;
}

static int ImagePage_Set_Amp(PROC_AMP_E AmpType, int value)
{
    printf("enter: %s, AmpType:%d value:%d\n", __func__, AmpType, value);

    int s32Ret = CVI_SUCCESS;
    int vpssGrp = 0;

    if (100 < value) {
        value = 100;
    } else if (0 > value) {
        value = 0;
    }

    s32Ret = CVI_VPSS_SetGrpProcAmp(vpssGrp, AmpType, value);
    if (s32Ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "set vpss grp amp(%d) failed with %#x!\n", AmpType, s32Ret);
        return s32Ret;
    }

    return CVI_SUCCESS;
}

static int ImagePage_Get_Sharpen(void)
{
    int s32Ret = CVI_SUCCESS;

    int value = 0;
    ISP_SHARPEN_ATTR_S stDRCAttr = {0};
    APP_PARAM_VI_CTX_S *pstViParamCfg = app_ipcam_Vi_Param_Get();
    int viPipe = pstViParamCfg->astPipeInfo[0].aPipe[0];

    s32Ret = CVI_ISP_GetSharpenAttr(viPipe, &stDRCAttr);
    if (s32Ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "get sharpen info failed with %#x!\n", s32Ret);
        return s32Ret;
    }

    if (stDRCAttr.Enable && (stDRCAttr.enOpType == OP_TYPE_MANUAL)) {
        value = stDRCAttr.stManual.GlobalGain;
    }

    return value;
}

static int ImagePage_Set_Sharpness(int value)
{
    printf("enter: %s, %d\n", __func__, value);
    ISP_SHARPEN_ATTR_S stDRCAttr;
    int ret;
    APP_PARAM_VI_CTX_S *pstViParamCfg = app_ipcam_Vi_Param_Get();
    int viPipe = pstViParamCfg->astPipeInfo[0].aPipe[0];

    ret = CVI_ISP_GetSharpenAttr(viPipe, &stDRCAttr);
    if (ret != CVI_SUCCESS) {
        printf("CVI_ISP_GetSharpenAttr failed\n");
        return -1;
    }

    stDRCAttr.Enable = CVI_TRUE;
    stDRCAttr.enOpType = OP_TYPE_MANUAL;
    stDRCAttr.stManual.GlobalGain = (CVI_U8)value;

    ret = CVI_ISP_SetSharpenAttr(viPipe, &stDRCAttr);
    if (ret != CVI_SUCCESS) {
        printf("CVI_ISP_SetSharpenAttr failed\n");
    }

    return ret;
}

static int ImagePage_Get_2DNR(void)
{
    return s_noise2d;
}

static int ImagePage_Set_2DNR(int value)
{
    printf("enter: %s, %d\n", __func__, value);
    s_noise2d = value;
    return 0;
}

static int ImagePage_Get_3DNR(void)
{
    return s_noise3d;
}

static int ImagePage_Set_3DNR(int value)
{
    CVI_S32 ret;
    ISP_TNR_ATTR_S nioseTnrAttr;
    s_noise3d = value;
    printf("enter: %s, %d\n", __func__, value);
    ret = CVI_ISP_GetTNRAttr(0, &nioseTnrAttr);
    if (ret != CVI_SUCCESS) {
        printf("CVI_ISP_GetTNRAttr failed\n");
    }
    for (int i = 0; i < ISP_AUTO_ISO_STRENGTH_NUM; i++) {
        APP_PROF_LOG_PRINT(LEVEL_INFO, "TnrStrength0[%d]=%d\n", i, nioseTnrAttr.stAuto.TnrStrength0[i]);
        nioseTnrAttr.stAuto.TnrStrength0[i] = s_noise3d;
    }

    ret = CVI_ISP_SetTNRAttr(0, &nioseTnrAttr);
    if (ret != CVI_SUCCESS) {
        printf("CVI_ISP_SetTNRAttr failed\n");
    }
    return 0;
}

static int ImagePage_Get_Compensation_Enable(void)
{
    return s_compensationEnable;
}

static int ImagePage_Set_Compensation_Enable(int value)
{
    printf("enter: %s, %d\n", __func__, value);
    s_compensationEnable = value;
    return 0;
}

static int ImagePage_Get_Compensation(void)
{
    return s_compensation;
}

static int ImagePage_Set_Compensation(int value)
{
    printf("enter: %s, %d\n", __func__, value);
    s_compensation = value;
    return 0;
}

static int ImagePage_Get_Suppression_Enable(void)
{
    return s_suppressionEnabled;
}

static int ImagePage_Set_Suppression_Enable(int value)
{
    printf("enter: %s, %d\n", __func__, value);
    s_suppressionEnabled = value;
    return 0;
}

static int ImagePage_Get_Suppression(void)
{
    return s_suppression;
}

static int ImagePage_Set_Suppression(int value)
{
    printf("enter: %s, %d\n", __func__, value);
    s_suppression = value;
    return 0;
}

static int ImagePage_Get_WB(void)
{
    return s_whiteBalance;
}

static int ImagePage_Set_WB(int value)
{
    printf("enter: %s, %d\n", __func__, value);
    s_whiteBalance = value;
    return 0;
}

static int ImagePage_Get_RedGain(void)
{
    return s_redGain;
}

static int ImagePage_Set_RedGain(int value)
{
    printf("enter: %s, %d\n", __func__, value);
    s_blueGain = value;
    return 0;
}

static int ImagePage_Get_BlueGain(void)
{
    return s_blueGain;
}

static int ImagePage_Set_BlueGain(int value)
{
    printf("enter: %s, %d\n", __func__, value);
    s_redGain = value;
    return 0;
}

static int ImagePage_Get_Defog_Enable(void)
{
    return s_defogEnabled;
}

static int ImagePage_Set_Defog_Enable(int value)
{
    CVI_S32 ret;
    VI_PIPE viPipe = 0;
    ISP_DEHAZE_ATTR_S dehazeAttr;
    printf("enter: %s, %d\n", __func__, value);
    s_defogEnabled = value;
    if (s_shutterEnabled == CVI_TRUE) {
        CVI_ISP_GetDehazeAttr(viPipe, &dehazeAttr);
        dehazeAttr.Enable = s_defogEnabled;
        ret = CVI_ISP_SetDehazeAttr(viPipe, &dehazeAttr);
        if (ret != CVI_SUCCESS) {
            printf("CVI_ISP_SetDehazeAttr failed\n");
        }
    } else {
        printf("shutter\n");
    }
    return 0;
}

static int ImagePage_Get_Defog(void)
{
    return s_defog;
}

static int ImagePage_Set_Defog(int value)
{
    // VI_PIPE viPipe = 0;
    // ISP_DEHAZE_ATTR_S dehazeAttr;
    printf("enter: %s, %d\n", __func__, value);
    s_defog = value;
    /*
    if (s_shutterEnabled == CVI_TRUE) {
        CVI_ISP_GetDehazeAttr(viPipe, &dehazeAttr);
        dehazeAttr.stAuto.Strength = s_defog;           // 0-100
        CVI_ISP_SetDehazeAttr(viPipe, &dehazeAttr);
    } else {
        printf("Defog hasen't enable\n");
    }
    */
    return 0;
}

static int ImagePage_Get_Shutter_Enable(void)
{
    return s_shutterEnabled;
}

static int ImagePage_Set_Shutter_Enable(int value)
{
    CVI_S32 ret;
    VI_PIPE viPipe = 0;
    ISP_EXPOSURE_ATTR_S aeAttr;
    printf("enter: %s, %d\n", __func__, value);
    s_shutterEnabled = value;

    CVI_ISP_GetExposureAttr(viPipe, &aeAttr);
    if (s_shutterEnabled == CVI_TRUE) {
        aeAttr.enOpType = OP_TYPE_MANUAL;
    } else {
        aeAttr.enOpType = OP_TYPE_AUTO;
    }
    ret = CVI_ISP_SetExposureAttr(viPipe, &aeAttr);
    if (ret != CVI_SUCCESS) {
        printf("CVI_ISP_SetExposureAttr failed\n");
    }

    return 0;
}

static int ImagePage_Get_Shutter(void)
{
    return s_shutter;
}

static int ImagePage_Set_Shutter(int value)
{
    CVI_S32 ret;
    VI_PIPE viPipe = 0;
    ISP_EXPOSURE_ATTR_S aeAttr;
    printf("enter: %s, %d\n", __func__, value);
    s_shutter = value;
    if (s_shutterEnabled == CVI_TRUE) {
        CVI_ISP_GetExposureAttr(viPipe, &aeAttr);
        aeAttr.bByPass = CVI_FALSE;  
        aeAttr.stManual.enExpTimeOpType = OP_TYPE_MANUAL;
        aeAttr.stManual.u32ExpTime = s_shutter;
        ret = CVI_ISP_GetExposureAttr(viPipe, &aeAttr);
        if (ret != CVI_SUCCESS) {
            printf("CVI_ISP_GetExposureAttr failed\n");
        }
    } else {
        printf("shutter MANUAL hasen't enabled\n");
    }
    return 0;
}

static int ImagePage_Get_Distortion_Enable(void)
{
    return s_distortionEnabled;
}

static int ImagePage_Set_Distortion_Enable(int value)
{
    printf("enter: %s, %d\n", __func__, value);

#if 0
    VI_PIPE viPipe = 0;
    VI_CHN viChn = 0;
    VI_LDC_ATTR_S setLDCAttr;
    printf("enter: %s, %d\n", __func__, value);
    s_distortionEnabled = value;
    CVI_VI_GetChnLDCAttr(viPipe, viChn, &setLDCAttr);
    setLDCAttr.bEnable = s_distortionEnabled;
    CVI_VI_SetChnLDCAttr(viPipe, viChn, &setLDCAttr);
#endif

    return 0;
}

static int ImagePage_Get_Distortion(void)
{
    return s_distortion;
}

static int ImagePage_Set_Distortion(int value)
{
    printf("enter: %s, %d\n", __func__, value);
#if 0
    VI_PIPE viPipe = 0;
    VI_CHN viChn = 0;
    VI_LDC_ATTR_S setLDCAttr;
    CVI_S32 ldcRatio[5] = {-300, -150, 0, 150, 300};
    printf("enter: %s, %d\n", __func__, value);
    s_distortion = value;
    CVI_VI_GetChnLDCAttr(viPipe, viChn, &setLDCAttr);
    setLDCAttr.stAttr.s32CenterXOffset = 0;
    setLDCAttr.stAttr.s32CenterYOffset = 0;
    setLDCAttr.stAttr.s32DistortionRatio = ldcRatio[s_distortion - 1];
    CVI_VI_SetChnLDCAttr(viPipe, viChn, &setLDCAttr);
#endif
    return 0;
}

static int ImagePage_Get_Frequency(void)
{
    return s_frequency;
}

static int ImagePage_Set_Frequency(int value)
{
    printf("enter: %s, %d\n", __func__, value);
    s_frequency = value;
    return 0;
}

static int ImagePage_Get_Antiflash(void)
{
    return s_antiflash;
}

static int ImagePage_Set_Antiflash(int value)
{
    printf("enter: %s, %d\n", __func__, value);
    s_antiflash = value;
    return 0;
}

static int ImagePage_Get_Rotation(APP_ROTATION_E eRType)
{
    CVI_S32 s32Ret;
    CVI_BOOL bMirror, bFlip;
    int value = 0;
    APP_PARAM_VI_CTX_S *pstViParamCfg = app_ipcam_Vi_Param_Get();
    int viPipe = pstViParamCfg->astPipeInfo[0].aPipe[0];
    int viChn = pstViParamCfg->astChnInfo[0].s32ChnId;

    s32Ret = CVI_VI_GetChnFlipMirror(viPipe, viChn, &bFlip, &bMirror);
    if (s32Ret != CVI_SUCCESS) {
        printf("CVI_VI_GetChnFlipMirror failed\n");
        return -1;
    }

    if (eRType == APP_ENUM_MIRROR) {
        value = bMirror ? 1 : 0;
    } else if (eRType == APP_ENUM_FLIP) {
        value = bFlip ? 1 : 0;
    }

    return value;
}

static int ImagePage_Set_Rotation(APP_ROTATION_E eRType, int value)
{
    CVI_BOOL tmpFlip, tmpMirror;
    CVI_S32 s32Ret = CVI_SUCCESS;
    APP_PARAM_VI_CTX_S *pstViParamCfg = app_ipcam_Vi_Param_Get();
    int viPipe = pstViParamCfg->astPipeInfo[0].aPipe[0];
    int viChn = pstViParamCfg->astChnInfo[0].s32ChnId;

    if (eRType > APP_ENUM_180) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "rotate type(%d) not right!\n", eRType);
        return -1;
    }

    s32Ret = CVI_VI_GetChnFlipMirror(viPipe, viChn, &tmpFlip, &tmpMirror);
    if (s32Ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "get chn rotate(%d) failed with %#x!\n", eRType, s32Ret);
        return s32Ret;
    }

    if (eRType == APP_ENUM_MIRROR) {
        if (tmpMirror == value) {
            return 0;
        } else {
            s32Ret = CVI_VI_SetChnFlipMirror(viPipe, viChn, tmpFlip, value);
        }
    } else if (eRType == APP_ENUM_FLIP) {
        if (tmpFlip == value) {
            return 0;
        } else {
            s32Ret = CVI_VI_SetChnFlipMirror(viPipe, viChn, value, tmpMirror);
        }
    } else if (eRType == APP_ENUM_180) {
        if ((tmpFlip == value) && (tmpMirror == value)) {
            return 0;
        } else {
            s32Ret = CVI_VI_SetChnFlipMirror(viPipe, viChn, value, value);
        }
    }
    if (s32Ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "set chn rotate(%d) failed with %#x!\n", eRType, s32Ret);
        return s32Ret;
    }

    return CVI_SUCCESS;
}

static int ImagePage_Get_WDR(void)
{
    int value = 0;
    APP_PARAM_VI_CTX_S *pstViParamCfg = app_ipcam_Vi_Param_Get();
    WDR_MODE_E enWDRMode = pstViParamCfg->astDevInfo[0].enWDRMode;

    if (enWDRMode == WDR_MODE_NONE) {
        value = 0;
    } else if (enWDRMode == WDR_MODE_2To1_LINE) {
        value = 1;
    }

    return value;
}

static int ImagePage_Set_WDR(int value)
{
    printf("enter: %s, %d\n", __func__, value);
    APP_PARAM_VI_CTX_S *pstViCtx = app_ipcam_Vi_Param_Get();

    app_ipcam_Vi_DeInit();

    if (value) {
        pstViCtx->astDevInfo[0].enWDRMode = WDR_MODE_2To1_LINE;
    } else {
        pstViCtx->astDevInfo[0].enWDRMode = WDR_MODE_NONE;
    }

    app_ipcam_Vi_Init();
    
    return 0;
}

static int ImagePage_Get_IRCut(void)
{
    return s_irCut;
}

static int ImagePage_Set_IRCut_Auto(int value)
{
    printf("enter: %s, %d\n", __func__, value);
    s_irCut = value;
    app_ipcam_IRCutMode_Select(value);
    return 0;
}

static int ImagePage_Set_IRCut_Manual(int value, int state)
{
    printf("enter: %s, %d\n", __func__, value);
    s_irCut = value;
    app_ipcam_IRCutMode_ManualCtrl(value, state);
    return 0;
}

static int ImagePage_Get_KeepColor(void)
{
    return s_keepColor;
}

static int ImagePage_Set_KeepColor(int value)
{
    printf("enter: %s, %d\n", __func__, value);
    s_keepColor = value;
    return 0;
}

static int ImagePage_Get_Dis(void)
{
    ISP_DIS_ATTR_S stDisAttr;

    CVI_ISP_GetDisAttr(0, &stDisAttr);

    return stDisAttr.enable;
}

static int ImagePage_Set_Dis(int value)
{
    printf("enter: %s, %d\n", __func__, value);
    ISP_DIS_ATTR_S stDisAttr;

    CVI_ISP_GetDisAttr(0, &stDisAttr);
    stDisAttr.enable = value;

    return CVI_ISP_SetDisAttr(0, &stDisAttr);

    return 0;
}

static int ImagePage_Get_DS(void)
{
    return s_ds;
}

static int ImagePage_Set_DS(int value)
{
    printf("enter: %s, %d\n", __func__, value);
    s_ds = value;
    return 0;
}

static int SetImgInfoCallBack(void *param, const char *cmd, const char *val)
{
    printf("enter: %s [%s|%s]\n", __func__, cmd, val);
    int ret = 0;
    int value = 0;
    static int irValue = 0;
    static int irState = 0;

    if (strstr(val, "brightness") != NULL) {
        value = NetString2Int(val);
        ImagePage_Set_Amp(PROC_AMP_BRIGHTNESS, value);
    } else if (strstr(val, "chroma") != NULL) {
        value = NetString2Int(val);
        ImagePage_Set_Amp(PROC_AMP_HUE, value);
    } else if (strstr(val, "contrast") != NULL) {
        value = NetString2Int(val);
        ImagePage_Set_Amp(PROC_AMP_CONTRAST, value);
    } else if (strstr(val, "saturation") != NULL) {
        value = NetString2Int(val);
        ImagePage_Set_Amp(PROC_AMP_SATURATION, value);
    } else if (strstr(val, "sharpness") != NULL) {
        value = NetString2Int(val);
        ImagePage_Set_Sharpness(value);
    } else if (strstr(val, "noise2d") != NULL) {
        value = NetString2Int(val);
        ImagePage_Set_2DNR(value);
    } else if (strstr(val, "noise3d") != NULL) {
        value = NetString2Int(val);
        ImagePage_Set_3DNR(value);
    } else if (strstr(val, "compensation_enable") != NULL) {
        value = NetString2Int(val);
        ImagePage_Set_Compensation_Enable(value);
    } else if (strstr(val, "compensation") != NULL) {
        value = NetString2Int(val);
        ImagePage_Set_Compensation(value);
    } else if (strstr(val, "suppression_enable") != NULL) {
        value = NetString2Int(val);
        ImagePage_Set_Suppression_Enable(value);
    } else if (strstr(val, "suppression") != NULL) {
        value = NetString2Int(val);
        ImagePage_Set_Suppression(value);
    } else if (strstr(val, "whiteBalance") != NULL) {
        value = NetString2Int(val);
        ImagePage_Set_WB(value);
    } else if (strstr(val, "blue_gain") != NULL) {
        value = NetString2Int(val);
        ImagePage_Set_RedGain(value);
    } else if (strstr(val, "red_gain") != NULL) {
        value = NetString2Int(val);
        ImagePage_Set_BlueGain(value);
    } else if (strstr(val, "defog_enable") != NULL) {
        value = NetString2Int(val);
        ImagePage_Set_Defog_Enable(value);
    } else if (strstr(val, "defog") != NULL) {
        value = NetString2Int(val);
        ImagePage_Set_Defog(value);
    } else if (strstr(val, "shutterEnabled") != NULL) {
        value = NetString2Int(val);
        ImagePage_Set_Shutter_Enable(value);
    } else if (strstr(val, "shutter") != NULL) {
        value = NetString2Int(val);
        ImagePage_Set_Shutter(value);
    } else if (strstr(val, "distortionEnabled") != NULL) {
        value = NetString2Int(val);
        ImagePage_Set_Distortion_Enable(value);
    } else if (strstr(val, "distortion") != NULL) {
        value = NetString2Int(val);
        ImagePage_Set_Distortion(value);
    } else if(strstr(val, "frequency") != NULL) {
        value = NetString2Int(val);
        ImagePage_Set_Frequency(value);
    } else if (strstr(val, "antiflash") != NULL) {
        value = NetString2Int(val);
        ImagePage_Set_Antiflash(value);
    } else if (strstr(val, "hflip") != NULL) {
        value = NetString2Int(val);
        ImagePage_Set_Rotation(APP_ENUM_MIRROR, value);
    } else if (strstr(val, "vflip") != NULL) {
        value = NetString2Int(val);
        ImagePage_Set_Rotation(APP_ENUM_FLIP, value);
    } else if (strstr(val, "wdr") != NULL) {
        value = NetString2Int(val);
        ImagePage_Set_WDR(value);
    } else if (strstr(val, "ircut") != NULL) {
        value = NetString2Int(val);
        ImagePage_Set_IRCut_Auto(value);
    } else if (strstr(val, "manual_ir") != NULL) {
        irValue = NetString2Int(val);
    } else if (strstr(val, "stateIR") != NULL) {
        irState = NetString2Int(val);
        ImagePage_Set_IRCut_Manual(irValue, irState);
    } else if (strstr(val, "keepcolor") != NULL) {
        value = NetString2Int(val);
        ImagePage_Set_KeepColor(value);
    } else if (strstr(val, "dis") != NULL) {
        value = NetString2Int(val);
        ImagePage_Set_Dis(value);
    } else if (strstr(val, "ds") != NULL) {
        value = NetString2Int(val);
        ImagePage_Set_DS(value);
    } else {
        printf("%s %d: error no support setting, %s %s\n", __func__, __LINE__, cmd, val);
    }
    CVI_NET_AddCgiResponse(param, "ret: %d\n", ret);

    return ret;
}

static int app_ipcam_ImagePageInfo_Get(CVI_IMG_INFO_S *info)
{
    info->brightness   = ImagePage_Get_Amp(PROC_AMP_BRIGHTNESS);
    info->contrast     = ImagePage_Get_Amp(PROC_AMP_CONTRAST);
    info->chroma       = ImagePage_Get_Amp(PROC_AMP_HUE);
    info->saturation   = ImagePage_Get_Amp(PROC_AMP_SATURATION);
    info->sharpness    = ImagePage_Get_Sharpen();
     /* not support yet */
    info->noise2d      = ImagePage_Get_2DNR();
    info->noise3d      = ImagePage_Get_3DNR();
    info->compensationEnabled = ImagePage_Get_Compensation_Enable();
    info->compensation = ImagePage_Get_Compensation();
    info->suppressionEnabled = ImagePage_Get_Suppression_Enable();
    info->suppression  = ImagePage_Get_Suppression();
    info->whitebalance = ImagePage_Get_WB();
    info->red_gain     = ImagePage_Get_RedGain();
    info->blue_gain    = ImagePage_Get_BlueGain();
    info->defogEnabled = ImagePage_Get_Defog_Enable();
    info->defog        = ImagePage_Get_Defog();
    info->shutterEnabled = ImagePage_Get_Shutter_Enable();
    info->shutter      = ImagePage_Get_Shutter();
    info->distortionEnabled = ImagePage_Get_Distortion_Enable();
    info->distortion   = ImagePage_Get_Distortion();
    info->frequency    = ImagePage_Get_Frequency();
    info->antiflashEnabled = ImagePage_Get_Antiflash();
    info->hflipEnabled = ImagePage_Get_Rotation(APP_ENUM_MIRROR);
    info->vflipEnabled = ImagePage_Get_Rotation(APP_ENUM_FLIP);
    info->wdrEnabled   = ImagePage_Get_WDR();
    info->irCutEnabled = ImagePage_Get_IRCut();
    info->keepColorEnabled = ImagePage_Get_KeepColor();
    info->disEnabled   = ImagePage_Get_Dis();
    /* digital signature ; not support yet */
    info->dsEnabled    = ImagePage_Get_DS();

    return CVI_SUCCESS;
}

static int GetImgInfoCallBack(void *param, const char *cmd, const char *val)
{
    printf("enter: %s [%s|%s]\n", __func__, cmd, val);

    int ret = 0;
    char* str = NULL;
    cJSON* cjsonImgInfo = NULL;
    cjsonImgInfo = cJSON_CreateObject();

    CVI_IMG_INFO_S imgInfo = {0};

    ret = app_ipcam_ImagePageInfo_Get(&imgInfo);
    if (ret != 0) {
        printf("get image info failed\n");
        CVI_NET_AddCgiResponse(param, "%s", "get image info failed");
    }

    cJSON_AddNumberToObject(cjsonImgInfo, "brightness", imgInfo.brightness);
    cJSON_AddNumberToObject(cjsonImgInfo, "sharpness", imgInfo.sharpness);
    cJSON_AddNumberToObject(cjsonImgInfo, "chroma", imgInfo.chroma);
    cJSON_AddNumberToObject(cjsonImgInfo, "contrast", imgInfo.contrast);
    cJSON_AddNumberToObject(cjsonImgInfo, "saturation", imgInfo.saturation);
    cJSON_AddNumberToObject(cjsonImgInfo, "noise2d", imgInfo.noise2d);
    cJSON_AddNumberToObject(cjsonImgInfo, "noise3d", imgInfo.noise3d);
    cJSON_AddNumberToObject(cjsonImgInfo, "compensationEnabled", imgInfo.compensationEnabled);
    cJSON_AddNumberToObject(cjsonImgInfo, "compensation", imgInfo.compensation);
    cJSON_AddNumberToObject(cjsonImgInfo, "suppressionEnabled", imgInfo.suppressionEnabled);
    cJSON_AddNumberToObject(cjsonImgInfo, "suppression", imgInfo.suppression);
    cJSON_AddNumberToObject(cjsonImgInfo, "whiteBalance", imgInfo.whitebalance);
    cJSON_AddNumberToObject(cjsonImgInfo, "red_gain", imgInfo.red_gain);
    cJSON_AddNumberToObject(cjsonImgInfo, "blue_gain", imgInfo.blue_gain);
    cJSON_AddNumberToObject(cjsonImgInfo, "defogEnabled", imgInfo.defogEnabled);
    cJSON_AddNumberToObject(cjsonImgInfo, "defog", imgInfo.defog);
    cJSON_AddNumberToObject(cjsonImgInfo, "shutterEnabled", imgInfo.shutterEnabled);
    cJSON_AddNumberToObject(cjsonImgInfo, "shutter", imgInfo.shutter);
    cJSON_AddNumberToObject(cjsonImgInfo, "distortionEnabled", imgInfo.distortionEnabled);
    cJSON_AddNumberToObject(cjsonImgInfo, "distortion", imgInfo.distortion);

    cJSON_AddNumberToObject(cjsonImgInfo, "frequency", imgInfo.frequency);
    cJSON_AddNumberToObject(cjsonImgInfo, "antiflashEnabled", imgInfo.antiflashEnabled);
    cJSON_AddNumberToObject(cjsonImgInfo, "hflipEnabled", imgInfo.hflipEnabled);
    cJSON_AddNumberToObject(cjsonImgInfo, "vflipEnabled", imgInfo.vflipEnabled);
    cJSON_AddNumberToObject(cjsonImgInfo, "wdrEnabled", imgInfo.wdrEnabled);
    cJSON_AddNumberToObject(cjsonImgInfo, "irCutEnabled", imgInfo.irCutEnabled);
    cJSON_AddNumberToObject(cjsonImgInfo, "irCutEnabledManual", imgInfo.irCutEnabledManual);
    cJSON_AddNumberToObject(cjsonImgInfo, "keepColorEnabled", imgInfo.keepColorEnabled);
    cJSON_AddNumberToObject(cjsonImgInfo, "disEnabled", imgInfo.disEnabled);
    cJSON_AddNumberToObject(cjsonImgInfo, "dsEnabled", imgInfo.dsEnabled);
    str = cJSON_Print(cjsonImgInfo);
    if (str) {
        CVI_NET_AddCgiResponse(param, "%s", str);
        cJSON_free(str);
    }

    cJSON_Delete(cjsonImgInfo);
    return 0;
}

/*
*   image page end
*/

/*
*   video page start
*/

static int app_ipcam_VencAttr_Get(APP_VENC_ATTR_INFO_S stVencAttrInfo[])
{
    CVI_U32 u32PicWidth;
    CVI_U32 u32PicHeight;
    int VencChn;
    VENC_CHN_ATTR_S stChnAttr = {0};
    APP_PARAM_SYS_CFG_S *pstSysCfg = app_ipcam_Sys_Param_Get();
    CVI_BOOL bSBMEnable = pstSysCfg->bSBMEnable;
    for (VencChn = 0; VencChn < APP_VENC_CHN_NUM; VencChn++) {
        memset(&stVencAttrInfo[VencChn], 0, sizeof(APP_VENC_ATTR_INFO_S));
        memset(&stChnAttr, 0, sizeof(VENC_CHN_ATTR_S));
        if (CVI_VENC_GetChnAttr(VencChn, &stChnAttr)) {
            printf("CVI_VENC_GetChnAttr failed\n");
            return -1;
        }

        u32PicWidth = stChnAttr.stVencAttr.u32PicWidth;
        u32PicHeight = stChnAttr.stVencAttr.u32PicHeight;

        stVencAttrInfo[VencChn].chn = VencChn;
        stVencAttrInfo[VencChn].enabled = 0;
        stVencAttrInfo[VencChn].widthMax = u32PicWidth;
        stVencAttrInfo[VencChn].heightMax = u32PicHeight;

        switch (stChnAttr.stVencAttr.enType) {
        case PT_H264:
            stVencAttrInfo[VencChn].codec = APP_VIDEOENCTYPE_H264;
            break;
        case PT_H265:
            stVencAttrInfo[VencChn].codec = APP_VIDEOENCTYPE_H265;
            break;
        case PT_MJPEG:
            stVencAttrInfo[VencChn].codec = APP_VIDEOENCTYPE_MJPEG;
            break;
        case PT_JPEG:
            stVencAttrInfo[VencChn].codec = APP_VIDEOENCTYPE_JPEG;
            break;
        default:
            break;
        }

        APP_RESOLUTION_INDEX_GET(VencChn, u32PicWidth, u32PicHeight, stVencAttrInfo[VencChn].resolution);

        if(stChnAttr.stVencAttr.enType == PT_H264) {
            switch (stChnAttr.stVencAttr.u32Profile) {
            case H264E_PROFILE_BASELINE:
                stVencAttrInfo[VencChn].profile = APP_VIDEOPROFILE_BASE;
                break;
            case H264E_PROFILE_MAIN:
                stVencAttrInfo[VencChn].profile = APP_VIDEOPROFILE_MAIN;
                break;
            case H264E_PROFILE_HIGH:
                stVencAttrInfo[VencChn].profile = APP_VIDEOPROFILE_HIGH;
                break;
            default:
                break;
            }
        } else{
            stVencAttrInfo[VencChn].profile = APP_VIDEOPROFILE_MAIN;
        }

        switch (stChnAttr.stRcAttr.enRcMode) {
            case VENC_RC_MODE_H264CBR:
                stVencAttrInfo[VencChn].fps = stChnAttr.stRcAttr.stH264Cbr.fr32DstFrameRate;
                stVencAttrInfo[VencChn].rc = APP_VIDEORCMODE_CBR;
                stVencAttrInfo[VencChn].bitrate = stChnAttr.stRcAttr.stH264Cbr.u32BitRate;
                break;
            case VENC_RC_MODE_H264VBR:
                stVencAttrInfo[VencChn].fps = stChnAttr.stRcAttr.stH264Vbr.fr32DstFrameRate;
                stVencAttrInfo[VencChn].rc = APP_VIDEORCMODE_VBR;
                stVencAttrInfo[VencChn].bitrate = stChnAttr.stRcAttr.stH264Vbr.u32MaxBitRate;
                break;
            case VENC_RC_MODE_H264AVBR:
                stVencAttrInfo[VencChn].fps = stChnAttr.stRcAttr.stH264AVbr.fr32DstFrameRate;
                stVencAttrInfo[VencChn].rc = APP_VIDEORCMODE_AVBR;
                stVencAttrInfo[VencChn].bitrate = stChnAttr.stRcAttr.stH264AVbr.u32MaxBitRate;
                break;
            case VENC_RC_MODE_H265CBR:
                stVencAttrInfo[VencChn].fps = stChnAttr.stRcAttr.stH265Cbr.fr32DstFrameRate;
                stVencAttrInfo[VencChn].rc = APP_VIDEORCMODE_CBR;
                stVencAttrInfo[VencChn].bitrate = stChnAttr.stRcAttr.stH265Cbr.u32BitRate;
                break;
            case VENC_RC_MODE_H265VBR:
                stVencAttrInfo[VencChn].fps = stChnAttr.stRcAttr.stH265Vbr.fr32DstFrameRate;
                stVencAttrInfo[VencChn].rc = APP_VIDEORCMODE_VBR;
                stVencAttrInfo[VencChn].bitrate = stChnAttr.stRcAttr.stH265Vbr.u32MaxBitRate;
                break;
            case VENC_RC_MODE_H265AVBR:
                stVencAttrInfo[VencChn].fps = stChnAttr.stRcAttr.stH265AVbr.fr32DstFrameRate;
                stVencAttrInfo[VencChn].rc = APP_VIDEORCMODE_AVBR;
                stVencAttrInfo[VencChn].bitrate = stChnAttr.stRcAttr.stH265AVbr.u32MaxBitRate;
                break;
            default:
                break;
        }

        if (stVencAttrInfo[VencChn].fps == -1 || bSBMEnable) {
            stVencAttrInfo[VencChn].fps = app_ipcam_Framerate_Get(0);
        }
    }

    return 0;
}

static int app_ipcam_VencBitrate_Set(VENC_CHN vencChn, CVI_U32 u32BitRate)
{
    VENC_CHN_ATTR_S stChnAttr = {0};
    memset(&stChnAttr, 0, sizeof(VENC_CHN_ATTR_S));
    if (CVI_VENC_GetChnAttr(vencChn, &stChnAttr)) {
        printf("CVI_VENC_GetChnAttr failed\n");
        return -1;
    }

    switch (stChnAttr.stRcAttr.enRcMode) {
        case VENC_RC_MODE_H264CBR:
            stChnAttr.stRcAttr.stH264Cbr.u32BitRate = u32BitRate;
            break;
        case VENC_RC_MODE_H264VBR:
            stChnAttr.stRcAttr.stH264Vbr.u32MaxBitRate = u32BitRate;
            break;
        case VENC_RC_MODE_H264AVBR:
            stChnAttr.stRcAttr.stH264AVbr.u32MaxBitRate = u32BitRate;
            break;
        case VENC_RC_MODE_H265CBR:
            stChnAttr.stRcAttr.stH265Cbr.u32BitRate = u32BitRate;
            break;
        case VENC_RC_MODE_H265VBR:
            stChnAttr.stRcAttr.stH265Vbr.u32MaxBitRate = u32BitRate;
            break;
        case VENC_RC_MODE_H265AVBR:
            stChnAttr.stRcAttr.stH265AVbr.u32MaxBitRate = u32BitRate;
            break;
        default:
            break;
    }
    
    if (CVI_VENC_SetChnAttr(vencChn, &stChnAttr)) {
        printf("CVI_VENC_GetChnAttr failed\n");
        return -1;
    }

    return 0;
}

static int app_ipcam_RoiCfg_Get(VENC_CHN vencChn, int iRoiIndex, APP_VENC_ROI_CFG_S *pstRoiAttr)
{
    int ret = 0;
    VENC_ROI_ATTR_S roiAttr;
    memset(&roiAttr, 0, sizeof(roiAttr));
    ret = CVI_VENC_GetRoiAttr(vencChn, iRoiIndex, &roiAttr);
    if (ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "GetRoiAttr failed!\n");
        return CVI_FAILURE;
    }

    pstRoiAttr->bEnable = roiAttr.bEnable;
    if (pstRoiAttr->bEnable) {
        pstRoiAttr->VencChn = vencChn;
        pstRoiAttr->bAbsQp = roiAttr.bAbsQp ? 1 : 0;
        pstRoiAttr->u32Qp = roiAttr.s32Qp;
        pstRoiAttr->u32Width = roiAttr.stRect.u32Width;
        pstRoiAttr->u32Height = roiAttr.stRect.u32Height;
        pstRoiAttr->u32X = roiAttr.stRect.s32X;
        pstRoiAttr->u32Y = roiAttr.stRect.s32Y;
        pstRoiAttr->u32Index = roiAttr.u32Index;
    } else {
        return CVI_FAILURE;
    }
    return 0;
}

static int app_ipcam_RoiCfg_Set(APP_VENC_ROI_CFG_S *pstRoiAttr)
{
    if (NULL == pstRoiAttr)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "pstRoiAttr is NULL!\n");
        return CVI_FAILURE;
    }

    CVI_S32 i = 0;
    CVI_S32 ret = 0;
    VENC_ROI_ATTR_S roiAttr;
    APP_PARAM_VENC_CTX_S *pstVencCtx = app_ipcam_Venc_Param_Get();

    for (i = 0; i < MAX_NUM_ROI; i++) {
        if ((pstVencCtx->astVencChnCfg[pstRoiAttr[i].VencChn].bEnable == 0) ||
            (pstVencCtx->astVencChnCfg[pstRoiAttr[i].VencChn].enType != PT_H264)) {
            continue;
        }

        memset(&roiAttr, 0, sizeof(roiAttr));
        roiAttr.bEnable = pstRoiAttr[i].bEnable;
        roiAttr.bAbsQp = pstRoiAttr[i].bAbsQp;
        roiAttr.s32Qp = pstRoiAttr[i].u32Qp;
        roiAttr.u32Index = i;
        roiAttr.stRect.s32X = pstRoiAttr[i].u32X;
        roiAttr.stRect.s32Y = pstRoiAttr[i].u32Y;
        roiAttr.stRect.u32Width = pstRoiAttr[i].u32Width;
        roiAttr.stRect.u32Height = pstRoiAttr[i].u32Height;
        ret = CVI_VENC_SetRoiAttr(pstRoiAttr[i].VencChn, &roiAttr);
        if (ret != CVI_SUCCESS)  {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "SetRoiAttr failed!\n");
            return CVI_FAILURE;
        }
    }

    return CVI_SUCCESS;
}


static int GetStreamCfgCallBack(void *param, const char *cmd, const char *val)
{
    printf("enter: %s [%s|%s]\n", __func__, cmd, val);
    int ret = 0;
    cJSON* cJsonRoot = NULL;
    char* str = NULL;

    APP_VENC_ATTR_INFO_S info[APP_VENC_CHN_NUM] = {0};
    ret = app_ipcam_VencAttr_Get(info);

    cJsonRoot = cJSON_CreateObject();
    cJSON_AddNumberToObject(cJsonRoot, "main_enabled", info[0].enabled);
    cJSON_AddNumberToObject(cJsonRoot, "main_codec", info[0].codec);
    cJSON_AddNumberToObject(cJsonRoot, "main_resolution", info[0].resolution);
    cJSON_AddNumberToObject(cJsonRoot, "main_widthMax", info[0].widthMax);
    cJSON_AddNumberToObject(cJsonRoot, "main_heightMax", info[0].heightMax);
    cJSON_AddNumberToObject(cJsonRoot, "main_fps", info[0].fps);
    cJSON_AddNumberToObject(cJsonRoot, "main_profile", info[0].profile);
    cJSON_AddNumberToObject(cJsonRoot, "main_rc", info[0].rc);
    cJSON_AddNumberToObject(cJsonRoot, "main_bitrate", info[0].bitrate);
    cJSON_AddNumberToObject(cJsonRoot, "sub_enabled", info[1].enabled);
    cJSON_AddNumberToObject(cJsonRoot, "sub_codec", info[1].codec);
    cJSON_AddNumberToObject(cJsonRoot, "sub_resolution", info[1].resolution);
    cJSON_AddNumberToObject(cJsonRoot, "sub_widthMax", info[1].widthMax);
    cJSON_AddNumberToObject(cJsonRoot, "sub_heightMax", info[1].heightMax);
    cJSON_AddNumberToObject(cJsonRoot, "sub_fps", info[1].fps);
    cJSON_AddNumberToObject(cJsonRoot, "sub_profile", info[1].profile);
    cJSON_AddNumberToObject(cJsonRoot, "sub_rc", info[1].rc);
    cJSON_AddNumberToObject(cJsonRoot, "sub_bitrate", info[1].bitrate);

    cJSON_AddNumberToObject(cJsonRoot, "record_status", 1);

    str = cJSON_Print(cJsonRoot);
    if (str) {
        printf("cJson: %s\n", str);
        CVI_NET_AddCgiResponse(param, "%s", str);
        cJSON_free(str);
    }

    cJSON_Delete(cJsonRoot);
    return ret;
}

static int GetRoiCfgCallBack(void *param, const char *cmd, const char *val)
{
    printf("enter: %s [%s|%s]\n", __func__, cmd, val);
    int ret = 0;
    cJSON* cJsonRoot = NULL;
    char* str = NULL;
    int i = 0;
    VENC_CHN vencChn = 0;
    char tmpStr[32];
    APP_PARAM_VENC_CTX_S *pstVencCtx = app_ipcam_Venc_Param_Get();
    APP_VENC_ROI_CFG_S stRoiAttr;
    memset(&stRoiAttr, 0, sizeof(APP_VENC_ROI_CFG_S));

    cJsonRoot = cJSON_CreateObject();
    for (i = 0 ; i < MAX_NUM_ROI; i++) {
        for (vencChn = 0; vencChn < APP_VENC_CHN_NUM; vencChn++) {
            if ((pstVencCtx->astVencChnCfg[vencChn].bEnable == 0) ||
                (pstVencCtx->astVencChnCfg[vencChn].enType != PT_H264)) {
                continue;
            }
            if (0 == app_ipcam_RoiCfg_Get(vencChn, i, &stRoiAttr)) {
                break;
            }
        }

        memset(tmpStr, 0, sizeof(tmpStr));
        snprintf(tmpStr, sizeof(tmpStr), "%s%d", "roi_enable", i);
        cJSON_AddNumberToObject(cJsonRoot, tmpStr, stRoiAttr.bEnable);

        memset(tmpStr, 0, sizeof(tmpStr));
        snprintf(tmpStr, sizeof(tmpStr), "%s%d", "roi_venc", i);
        cJSON_AddNumberToObject(cJsonRoot, tmpStr, stRoiAttr.VencChn);

        if (stRoiAttr.bEnable == 0) {
            memset(&stRoiAttr, 0, sizeof(APP_VENC_ROI_CFG_S));
        }
        memset(tmpStr, 0, sizeof(tmpStr));
        snprintf(tmpStr, sizeof(tmpStr), "%s%d", "roi_abs_qp", i);
        cJSON_AddNumberToObject(cJsonRoot, tmpStr, stRoiAttr.bAbsQp);
        
        memset(tmpStr, 0, sizeof(tmpStr));
        snprintf(tmpStr, sizeof(tmpStr), "%s%d", "roi_qp", i);
        cJSON_AddNumberToObject(cJsonRoot, tmpStr, stRoiAttr.u32Qp);

        memset(tmpStr, 0, sizeof(tmpStr));
        snprintf(tmpStr, sizeof(tmpStr), "%s%d", "roi_x", i);
        cJSON_AddNumberToObject(cJsonRoot, tmpStr, stRoiAttr.u32X);

        memset(tmpStr, 0, sizeof(tmpStr));
        snprintf(tmpStr, sizeof(tmpStr), "%s%d", "roi_y", i);
        cJSON_AddNumberToObject(cJsonRoot, tmpStr, stRoiAttr.u32Y);

        memset(tmpStr, 0, sizeof(tmpStr));
        snprintf(tmpStr, sizeof(tmpStr), "%s%d", "roi_width", i);
        cJSON_AddNumberToObject(cJsonRoot, tmpStr, stRoiAttr.u32Width);

        memset(tmpStr, 0, sizeof(tmpStr));
        snprintf(tmpStr, sizeof(tmpStr), "%s%d", "roi_hight", i);
        cJSON_AddNumberToObject(cJsonRoot, tmpStr, stRoiAttr.u32Height);
}
    str = cJSON_Print(cJsonRoot);
    if (str) {
        printf("cJson: %s\n", str);
        CVI_NET_AddCgiResponse(param, "%s", str);
        cJSON_free(str);
    }

    cJSON_Delete(cJsonRoot);
    return ret;
}

static CVI_BOOL app_ipcam_VencAttrChange_Check(APP_VENC_ATTR_INFO_S NewInfo[])
{
    CVI_BOOL bNeedResize = CVI_FALSE;
    APP_VENC_ATTR_INFO_S CurInfo[APP_VENC_CHN_NUM] = {0};

    app_ipcam_VencAttr_Get(CurInfo);

    int fps = app_ipcam_Framerate_Get(0);

    for (int i = 0; i < APP_VENC_CHN_NUM; i++) {
        APP_PARAM_SAME_CHK(CurInfo[i].resolution, NewInfo[i].resolution, stResetVencFlag[i].bResolution);
        APP_PARAM_SAME_CHK(CurInfo[i].codec, NewInfo[i].codec, stResetVencFlag[i].bCodec);
        APP_PARAM_SAME_CHK(CurInfo[i].profile, NewInfo[i].profile, stResetVencFlag[i].bProfile);
        APP_PARAM_SAME_CHK(CurInfo[i].bitrate, NewInfo[i].bitrate, stResetVencFlag[i].bBitrate);
        APP_PARAM_SAME_CHK(CurInfo[i].rc, NewInfo[i].rc, stResetVencFlag[i].bRCMode);
        APP_PARAM_SAME_CHK(fps, NewInfo[i].fps, stResetVencFlag[i].bFps);

        bNeedResize |= stResetVencFlag[i].bResolution | stResetVencFlag[i].bCodec | stResetVencFlag[i].bProfile 
            | stResetVencFlag[i].bBitrate | stResetVencFlag[i].bRCMode | stResetVencFlag[i].bFps;

        stResetVencFlag[i].bNeedStopVenc |= stResetVencFlag[i].bResolution | stResetVencFlag[i].bCodec
            | stResetVencFlag[i].bProfile | stResetVencFlag[i].bRCMode;
    }

    return bNeedResize;
}

static int app_ipcam_VencAttr_Set(APP_VENC_ATTR_INFO_S info[])
{
    // CVI_S32 ret;
    CVI_U32 VpssGrp;
    CVI_U32 VpssChn;

    APP_VENC_CHN_CFG_S *pstVencChnCfg = NULL;
    VPSS_CHN_ATTR_S *pVpssChnAttr = NULL;
    APP_VENC_CHN_E enVencChn = APP_VENC_NULL;
    for(int i = 0; i < APP_VENC_CHN_NUM; i++) {
        if (stResetVencFlag[i].bNeedStopVenc)
            enVencChn |= APP_VENC_1ST << i;
        printf("enVencChn=0x%x\n", enVencChn);
    }

    if (enVencChn != APP_VENC_NULL) {
        app_ipcam_VencResize_Stop(enVencChn, stResetVencFlag[APP_VENC_STREAM_SUB].bResolution);
    }

    for(int i = 0; i < APP_VENC_CHN_NUM; i++) {
        pstVencChnCfg = app_ipcam_VencChnCfg_Get(i);
        if (stResetVencFlag[i].bCodec || stResetVencFlag[i].bRCMode ) {
            switch (info[i].codec) {
            case APP_VIDEOENCTYPE_H264:
                pstVencChnCfg->enType = PT_H264;
                switch (info[i].rc) {
                case APP_VIDEORCMODE_CBR:
                    pstVencChnCfg->enRcMode = VENC_RC_MODE_H264CBR;
                    break;
                case APP_VIDEORCMODE_VBR:
                    pstVencChnCfg->enRcMode = VENC_RC_MODE_H264VBR;
                    break;
                case APP_VIDEORCMODE_AVBR:
                    pstVencChnCfg->enRcMode = VENC_RC_MODE_H264AVBR;
                    break;
                }

                break;
            case APP_VIDEOENCTYPE_H265:
                pstVencChnCfg->enType = PT_H265;
                switch (info[i].rc) {
                case APP_VIDEORCMODE_CBR:
                    pstVencChnCfg->enRcMode = VENC_RC_MODE_H265CBR;
                    break;
                case APP_VIDEORCMODE_VBR:
                    pstVencChnCfg->enRcMode = VENC_RC_MODE_H265VBR;
                    break;
                case APP_VIDEORCMODE_AVBR:
                    pstVencChnCfg->enRcMode = VENC_RC_MODE_H265AVBR;
                    break;
                }
                break;
            default:
                break;
            }
        }

        if (stResetVencFlag[i].bProfile) {
            if(pstVencChnCfg->enType == PT_H264) {
                switch (info[i].profile) {
                case APP_VIDEOPROFILE_BASE:
                    pstVencChnCfg->u32Profile = H264E_PROFILE_BASELINE;
                    break;
                case APP_VIDEOPROFILE_MAIN:
                    pstVencChnCfg->u32Profile = H264E_PROFILE_MAIN;
                    break;
                case APP_VIDEOPROFILE_HIGH:
                    pstVencChnCfg->u32Profile = H264E_PROFILE_HIGH;
                    break;
                default:
                    printf("%s cmd failed profile err %d\n", __func__, info[i].profile);
                    break;
                }
            }
        }

        if (stResetVencFlag[i].bResolution) {
            APP_RESOLUTION_GET(i, info[i].resolution, pstVencChnCfg->u32Width, pstVencChnCfg->u32Height);
            VpssGrp = pstVencChnCfg->VpssGrp;
            VpssChn = pstVencChnCfg->VpssChn;
            pVpssChnAttr = &app_ipcam_Vpss_Param_Get()->astVpssGrpCfg[VpssGrp].astVpssChnAttr[VpssChn];
            pVpssChnAttr->u32Width = pstVencChnCfg->u32Width;
            pVpssChnAttr->u32Height = pstVencChnCfg->u32Height;
            if (i == APP_VENC_STREAM_SUB) {
                APP_VENC_CHN_CFG_S *pstJpegVencChnCfg = app_ipcam_VencChnCfg_Get(APP_JPEG_VENC_CHN);
                pstJpegVencChnCfg->u32Width = pstVencChnCfg->u32Width;
                pstJpegVencChnCfg->u32Height = pstVencChnCfg->u32Height;
            }
            #ifdef ARCH_CV180X
            if (i == APP_VENC_STREAM_SUB) {
                #ifdef AI_SUPPORT
                APP_PARAM_AI_PD_CFG_S *pstAiPdCfg = app_ipcam_Ai_PD_Param_Get();
                APP_PARAM_AI_MD_CFG_S *pstAiMdCfg = app_ipcam_Ai_MD_Param_Get();
                VPSS_GRP VpssGrp_PD = pstAiPdCfg->VpssGrp;
                APP_VPSS_GRP_CFG_T *pstVpssGrpCfg = &app_ipcam_Vpss_Param_Get()->astVpssGrpCfg[VpssGrp_PD];
                pstVpssGrpCfg->stVpssGrpAttr.u32MaxW = pstVencChnCfg->u32Width;
                pstVpssGrpCfg->stVpssGrpAttr.u32MaxH = pstVencChnCfg->u32Height;
                VPSS_GRP VpssGrp_MD = pstAiMdCfg->VpssGrp;
                pstVpssGrpCfg = &app_ipcam_Vpss_Param_Get()->astVpssGrpCfg[VpssGrp_MD];
                pstVpssGrpCfg->stVpssGrpAttr.u32MaxW = pstVencChnCfg->u32Width;
                pstVpssGrpCfg->stVpssGrpAttr.u32MaxH = pstVencChnCfg->u32Height;
                #endif
            }
            #endif
        }

        if (stResetVencFlag[i].bBitrate) {
            pstVencChnCfg->u32MaxBitRate = info[i].bitrate;
            pstVencChnCfg->u32BitRate = info[i].bitrate;
            app_ipcam_VencBitrate_Set(i, info[i].bitrate);
        }
    }

    /* need re-start rtsp server when codec changed */
    for(int i = 0; i < APP_VENC_CHN_NUM; i++) {
        if (stResetVencFlag[i].bCodec) {
            app_ipcam_rtsp_Server_Destroy();
            app_ipcam_Rtsp_Server_Create();
            break;
        }
    }

    if (enVencChn != APP_VENC_NULL) {
        app_ipcam_VencResize_Start(enVencChn, stResetVencFlag[APP_VENC_STREAM_SUB].bResolution);
    }

    for(int i = 0; i < APP_VENC_CHN_NUM; i++) {
        if (stResetVencFlag[i].bFps) {
            app_ipcam_Framerate_Set(0, info[0].fps);
            break;
        }
    }
    return 0;
}

static int SetStreamCfgCallBack(void *param, const char *cmd, const char *val)
{
    printf("enter: %s [%s|%s]\n", __func__, cmd, val);
    int ret = 0;
    char decode[1024] = {0};
    cJSON *cJsonRoot = NULL;
    cJSON *cJsonObj = NULL;
    APP_VENC_ATTR_INFO_S info[APP_VENC_CHN_NUM] = {0};

    UrlDecode(val, decode);
    // printf("%s\n", decode);

    cJsonRoot = cJSON_Parse(decode);
    if (cJsonRoot == NULL) {
        printf("parse fail.\n");
        return -1;
    }

    cJsonObj = cJSON_GetObjectItem(cJsonRoot, "main_enabled");
    _NULL_POINTER_CHECK_(cJsonObj->valuestring, -1);
    info[0].enabled = atoi(cJsonObj->valuestring);
    cJsonObj = cJSON_GetObjectItem(cJsonRoot, "main_codec");
    _NULL_POINTER_CHECK_(cJsonObj->valuestring, -1);
    info[0].codec = atoi(cJsonObj->valuestring);
    cJsonObj = cJSON_GetObjectItem(cJsonRoot, "main_resolution");
    _NULL_POINTER_CHECK_(cJsonObj->valuestring, -1);
    info[0].resolution = atoi(cJsonObj->valuestring);
    cJsonObj = cJSON_GetObjectItem(cJsonRoot, "main_fps");
    _NULL_POINTER_CHECK_(cJsonObj->valuestring, -1);
    info[0].fps = atoi(cJsonObj->valuestring);
    cJsonObj = cJSON_GetObjectItem(cJsonRoot, "main_profile");
    _NULL_POINTER_CHECK_(cJsonObj->valuestring, -1);
    info[0].profile = atoi(cJsonObj->valuestring);
    cJsonObj = cJSON_GetObjectItem(cJsonRoot, "main_rc");
    _NULL_POINTER_CHECK_(cJsonObj->valuestring, -1);
    info[0].rc = atoi(cJsonObj->valuestring);
    cJsonObj = cJSON_GetObjectItem(cJsonRoot, "main_bitrate");
    _NULL_POINTER_CHECK_(cJsonObj->valuestring, -1);
    info[0].bitrate = atoi(cJsonObj->valuestring);

    cJsonObj = cJSON_GetObjectItem(cJsonRoot, "sub_enabled");
    _NULL_POINTER_CHECK_(cJsonObj->valuestring, -1);
    info[1].enabled = atoi(cJsonObj->valuestring);
    cJsonObj = cJSON_GetObjectItem(cJsonRoot, "sub_codec");
    _NULL_POINTER_CHECK_(cJsonObj->valuestring, -1);
    info[1].codec = atoi(cJsonObj->valuestring);
    cJsonObj = cJSON_GetObjectItem(cJsonRoot, "sub_resolution");
    _NULL_POINTER_CHECK_(cJsonObj->valuestring, -1);
    info[1].resolution = atoi(cJsonObj->valuestring);
    cJsonObj = cJSON_GetObjectItem(cJsonRoot, "sub_fps");
    _NULL_POINTER_CHECK_(cJsonObj->valuestring, -1);
    info[1].fps = atoi(cJsonObj->valuestring);
    cJsonObj = cJSON_GetObjectItem(cJsonRoot, "sub_profile");
    _NULL_POINTER_CHECK_(cJsonObj->valuestring, -1);
    info[1].profile = atoi(cJsonObj->valuestring);
    cJsonObj = cJSON_GetObjectItem(cJsonRoot, "sub_rc");
    _NULL_POINTER_CHECK_(cJsonObj->valuestring, -1);
    info[1].rc = atoi(cJsonObj->valuestring);
    cJsonObj = cJSON_GetObjectItem(cJsonRoot, "sub_bitrate");
    _NULL_POINTER_CHECK_(cJsonObj->valuestring, -1);
    info[1].bitrate = atoi(cJsonObj->valuestring);

    if (app_ipcam_VencAttrChange_Check(info)) {
        ret = app_ipcam_VencAttr_Set(info);
        memset(&stResetVencFlag, 0, sizeof(APP_VENC_ATTR_CHANGE_S) * APP_VENC_CHN_NUM);
    } else {
        printf("all video channel attr not change!\n");
    }
    CVI_NET_AddCgiResponse(param, "ret: %d\n", ret);
    cJSON_Delete(cJsonRoot);
    return ret;
}

static int SetRoiCfgCallBack(void *param, const char *cmd, const char *val)
{
    printf("enter: %s [%s|%s]\n", __func__, cmd, val);
    int ret = 0;
    char decode[1024] = {0};
    cJSON *cJsonRoot = NULL;
    cJSON *cJsonObj = NULL;
    // APP_VENC_ATTR_INFO_S info[APP_VENC_CHN_NUM] = {0};

    UrlDecode(val, decode);
    printf("%s\n", decode);

    cJsonRoot = cJSON_Parse(decode);
    if (cJsonRoot == NULL) {
        printf("parse fail.\n");
        return -1;
    }

    int i = 0;
    char tmpStr[32];
    // APP_PARAM_VENC_CTX_S *pstVencCtx = app_ipcam_Venc_Param_Get();
    APP_VENC_ROI_CFG_S stRoiAttr[MAX_NUM_ROI];
    for (i = 0; i < MAX_NUM_ROI; i++) {
        memset(tmpStr, 0, sizeof(tmpStr));
        snprintf(tmpStr, sizeof(tmpStr), "%s%d", "roi_enable", i);
        cJsonObj = cJSON_GetObjectItem(cJsonRoot, tmpStr);
        _NULL_POINTER_CHECK_(cJsonObj->valuestring, -1);
        stRoiAttr[i].bEnable = atoi(cJsonObj->valuestring);

        memset(tmpStr, 0, sizeof(tmpStr));
        snprintf(tmpStr, sizeof(tmpStr), "%s%d", "roi_venc", i);
        cJsonObj = cJSON_GetObjectItem(cJsonRoot, tmpStr);
        _NULL_POINTER_CHECK_(cJsonObj->valuestring, -1);
        stRoiAttr[i].VencChn = atoi(cJsonObj->valuestring);
        if (stRoiAttr[i].bEnable) {
            memset(tmpStr, 0, sizeof(tmpStr));
            snprintf(tmpStr, sizeof(tmpStr), "%s%d", "roi_abs_qp", i);
            cJsonObj = cJSON_GetObjectItem(cJsonRoot, tmpStr);
            _NULL_POINTER_CHECK_(cJsonObj->valuestring, -1);
            stRoiAttr[i].bAbsQp = atoi(cJsonObj->valuestring);
            
            memset(tmpStr, 0, sizeof(tmpStr));
            snprintf(tmpStr, sizeof(tmpStr), "%s%d", "roi_qp", i);
            cJsonObj = cJSON_GetObjectItem(cJsonRoot, tmpStr);
            _NULL_POINTER_CHECK_(cJsonObj->valuestring, -1);
            stRoiAttr[i].u32Qp = atoi(cJsonObj->valuestring);

            memset(tmpStr, 0, sizeof(tmpStr));
            snprintf(tmpStr, sizeof(tmpStr), "%s%d", "roi_x", i);
            cJsonObj = cJSON_GetObjectItem(cJsonRoot, tmpStr);
            _NULL_POINTER_CHECK_(cJsonObj->valuestring, -1);
            stRoiAttr[i].u32X = atoi(cJsonObj->valuestring);

            memset(tmpStr, 0, sizeof(tmpStr));
            snprintf(tmpStr, sizeof(tmpStr), "%s%d", "roi_y", i);
            cJsonObj = cJSON_GetObjectItem(cJsonRoot, tmpStr);
            _NULL_POINTER_CHECK_(cJsonObj->valuestring, -1);
            stRoiAttr[i].u32Y = atoi(cJsonObj->valuestring);

            memset(tmpStr, 0, sizeof(tmpStr));
            snprintf(tmpStr, sizeof(tmpStr), "%s%d", "roi_width", i);
            cJsonObj = cJSON_GetObjectItem(cJsonRoot, tmpStr);
            _NULL_POINTER_CHECK_(cJsonObj->valuestring, -1);
            stRoiAttr[i].u32Width = atoi(cJsonObj->valuestring);

            memset(tmpStr, 0, sizeof(tmpStr));
            snprintf(tmpStr, sizeof(tmpStr), "%s%d", "roi_hight", i);
            cJsonObj = cJSON_GetObjectItem(cJsonRoot, tmpStr);
            _NULL_POINTER_CHECK_(cJsonObj->valuestring, -1);
            stRoiAttr[i].u32Height = atoi(cJsonObj->valuestring);
        }
    }

    app_ipcam_RoiCfg_Set(stRoiAttr);
    CVI_NET_AddCgiResponse(param, "ret: %d\n", ret);
    cJSON_Delete(cJsonRoot);
    return ret;
}

#ifdef RECORD_SUPPORT
#if 0
static int GetRecordCallBack(void *param, const char *cmd, const char *val)
{
    printf("enter: %s [%s|%s]\n", __func__, cmd, val);
    int ret = 0;
    cJSON* cJsonRoot = NULL;
    char* str = NULL;

    char *tmpStr = NULL;
    app_ipcam_Get_Record_Check(&tmpStr);
    cJsonRoot = cJSON_CreateObject();
    if (NULL != tmpStr)
    {
        cJSON_AddStringToObject(cJsonRoot, "segment", tmpStr);
        tmpStr = NULL;
        app_ipcam_Clean_Record_Seg();
    }
    cJSON_AddNumberToObject(cJsonRoot, "start", app_ipcam_Get_Record_Status());

    str = cJSON_Print(cJsonRoot);
    if (str) {
        printf("cJson: %s\n", str);
        CVI_NET_AddCgiResponse(param, "%s", str);
        cJSON_free(str);
    }

    cJSON_Delete(cJsonRoot);
    return ret;
}

static int SetRecordCallBack(void *param, const char *cmd, const char *val)
{
    printf("enter: %s [%s|%s]\n", __func__, cmd, val);
    int ret = 0;
    int value = 0;
    if (strstr(val, "start") != NULL) {
        value = NetString2Int(val);
        app_ipcam_Set_Record_Status(value);
    } else if (strstr(val, "record_date") != NULL){
        char tmpStr[256];
        NetString2Str(val, tmpStr);
        if (strlen("20220709") == strlen(tmpStr))
        {
            app_ipcam_Set_Record_Check(tmpStr);
        }
        else
        {
            printf("date invalid record_date:%s\n", tmpStr);
        }
    } else {
        printf("%s %d: error no support setting, %s %s\n", __func__, __LINE__, cmd, val);
    }
    CVI_NET_AddCgiResponse(param, "ret: %d\n", ret);

    return ret;
}

static int StopRepalyCallBack(void *param, const char *cmd, const char *val)
{
    printf("enter: %s [%s|%s]\n", __func__, cmd, val);
    app_ipcam_Record_StopReplay();
    return 0;
}

static int StartRepalyCallBack(void *param, const char *cmd, const char *val)
{
    printf("enter: %s [%s|%s]\n", __func__, cmd, val);
    int ret = 0;
    if (strstr(val, "replay_date") != NULL){
        char tmpStr[256];
        NetString2Str(val, tmpStr);
        if (strlen("20210520141414") == strlen(tmpStr))
        {
            printf("replay_date:%s\n", tmpStr);
            app_ipcam_Record_StartReplay(tmpStr);
        }
        else
        {
            printf("date invalid replay_date:%s\n", tmpStr);
        }
    } else {
        printf("%s %d: error no support setting, %s %s\n", __func__, __LINE__, cmd, val);
    }
    CVI_NET_AddCgiResponse(param, "ret: %d\n", ret);

    return ret;
}
#endif
#endif

#ifdef AI_SUPPORT
int CVI_IPC_NetCtrlSetMd(APP_MD_INFO_S psmdinfo)
{
    if(app_ipcam_Ai_MD_ProcStatus_Get() != psmdinfo.enabled)
    {
        if (psmdinfo.enabled)
        {
            app_ipcam_Ai_MD_Start();
            return 0;
        }
        else 
        {
            app_ipcam_Ai_MD_Stop();
            return 0;
        }
    }
    if(app_ipcam_Ai_MD_Thresold_Get() != psmdinfo.threshold)
    {
        printf("md1:%d\n",app_ipcam_Ai_MD_Thresold_Get());
        app_ipcam_Ai_MD_Thresold_Set(psmdinfo.threshold);
        printf("md:%d\n",app_ipcam_Ai_MD_Thresold_Get());
        return 0;
    }
    
    return 0;
}
#if defined AUDIO_SUPPORT && defined AI_BABYCRY_SUPPORT



int CVI_IPC_NetCtrlSetCry(APP_CRY_INFO_S pscryinfo)
{
    if(app_ipcam_Ai_Cry_ProcStatus_Get() != pscryinfo.enabled)
    {
        if (pscryinfo.enabled)
        {
            app_ipcam_Ai_Cry_Start();
            return 0;
        }
        else 
        {
            app_ipcam_Ai_Cry_Stop();
            return 0;
        }
    }   
    return 0;
}
#endif
int CVI_IPC_NetCtrlSetPd(APP_PD_INFO_S pspdinfo)
{
    CVI_S32 s32Ret = CVI_SUCCESS;
    APP_PARAM_AI_PD_CFG_S *pstPdInfo = app_ipcam_Ai_PD_Param_Get();
    if(app_ipcam_Ai_PD_ProcStatus_Get() != pspdinfo.enabled)
    {
        if (pspdinfo.enabled)
        {
            app_ipcam_Ai_PD_Start();
            return 0;
        }
        else 
        {
            app_ipcam_Ai_PD_Stop();
            return 0;
        }
    }
       
    if (!((pstPdInfo->region_stRect_x1 == pspdinfo.region_stRect_x1) &&
        (pstPdInfo->region_stRect_y1 == pspdinfo.region_stRect_y1) &&
        (pstPdInfo->region_stRect_x2 == pspdinfo.region_stRect_x2) &&
        (pstPdInfo->region_stRect_y2 == pspdinfo.region_stRect_y2) &&
        (pstPdInfo->region_stRect_x3 == pspdinfo.region_stRect_x3) &&
        (pstPdInfo->region_stRect_y3 == pspdinfo.region_stRect_y3) &&
        (pstPdInfo->region_stRect_x4 == pspdinfo.region_stRect_x4) &&
        (pstPdInfo->region_stRect_y4 == pspdinfo.region_stRect_y4) &&
        (pstPdInfo->region_stRect_x5 == pspdinfo.region_stRect_x5) &&
        (pstPdInfo->region_stRect_y5 == pspdinfo.region_stRect_y5) &&
        (pstPdInfo->region_stRect_x6 == pspdinfo.region_stRect_x6) &&
        (pstPdInfo->region_stRect_y6 == pspdinfo.region_stRect_y6)) ||
        (pstPdInfo->Intrusion_bEnable != pspdinfo.Intrusion_enabled)) 
    {
        app_ipcam_Ai_PD_Stop();
        if(pstPdInfo->Intrusion_bEnable != pspdinfo.Intrusion_enabled)
        {
            pstPdInfo->Intrusion_bEnable = pspdinfo.Intrusion_enabled;
        }
        else{
            pstPdInfo->region_stRect_x1 = pspdinfo.region_stRect_x1;
            pstPdInfo->region_stRect_y1 = pspdinfo.region_stRect_y1;
            pstPdInfo->region_stRect_x2 = pspdinfo.region_stRect_x2;
            pstPdInfo->region_stRect_y2 = pspdinfo.region_stRect_y2;
            pstPdInfo->region_stRect_x3 = pspdinfo.region_stRect_x3;
            pstPdInfo->region_stRect_y3 = pspdinfo.region_stRect_y3;
            pstPdInfo->region_stRect_x4 = pspdinfo.region_stRect_x4;
            pstPdInfo->region_stRect_y4 = pspdinfo.region_stRect_y4;
            pstPdInfo->region_stRect_x5 = pspdinfo.region_stRect_x5;
            pstPdInfo->region_stRect_y5 = pspdinfo.region_stRect_y5;
            pstPdInfo->region_stRect_x6 = pspdinfo.region_stRect_x6;
            pstPdInfo->region_stRect_y6 = pspdinfo.region_stRect_y6;
        }
        app_ipcam_Ai_PD_Start();
        return 0;
    }
    
    if(pstPdInfo->threshold != pspdinfo.threshold)
    {
        pstPdInfo->threshold = pspdinfo.threshold;
        s32Ret = app_ipcam_Pd_threshold_Set(pstPdInfo->threshold);
        if (s32Ret != CVI_SUCCESS)
        {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "app_ipcam_Pd_threshold_Set failed with %#x!\n", s32Ret);
            return s32Ret;
        }
        return 0;
    }
    return 0;
}

static int GetAiInfoCallBack(void *param, const char *cmd, const char *val)
{
    cJSON* cjsonAiAttr = NULL;
    char* str = NULL;

    APP_PARAM_AI_PD_CFG_S *pstPdInfo = app_ipcam_Ai_PD_Param_Get();
    APP_PARAM_AI_MD_CFG_S *pstMdInfo = app_ipcam_Ai_MD_Param_Get();
    // APP_PARAM_AI_CRY_CFG_S *pstCryInfo = app_ipcam_Ai_Cry_Param_Get();
    cjsonAiAttr = cJSON_CreateObject();

    printf("enter: %s\n", __func__);
    // md
    cJSON_AddNumberToObject(cjsonAiAttr, "ai_model", 7);
    cJSON_AddNumberToObject(cjsonAiAttr, "md_enable", app_ipcam_Ai_MD_StatusGet());
    cJSON_AddNumberToObject(cjsonAiAttr, "md_threshold", pstMdInfo->threshold);

    // pd

    cJSON_AddNumberToObject(cjsonAiAttr, "pd_enable", app_ipcam_Ai_PD_StatusGet());
    cJSON_AddNumberToObject(cjsonAiAttr, "pd_intrusion_enable", app_ipcam_Ai_PD_StatusGet() ? pstPdInfo->Intrusion_bEnable : 0);
    cJSON_AddNumberToObject(cjsonAiAttr, "pd_threshold", (int)(pstPdInfo->threshold * 100));

    cJSON_AddNumberToObject(cjsonAiAttr, "region_x1",  pstPdInfo->region_stRect_x1);
    cJSON_AddNumberToObject(cjsonAiAttr, "region_y1",  pstPdInfo->region_stRect_y1);
    cJSON_AddNumberToObject(cjsonAiAttr, "region_x2",  pstPdInfo->region_stRect_x2);
    cJSON_AddNumberToObject(cjsonAiAttr, "region_y2",  pstPdInfo->region_stRect_y2);
    cJSON_AddNumberToObject(cjsonAiAttr, "region_x3",  pstPdInfo->region_stRect_x3);
    cJSON_AddNumberToObject(cjsonAiAttr, "region_y3",  pstPdInfo->region_stRect_y3);
    cJSON_AddNumberToObject(cjsonAiAttr, "region_x4",  pstPdInfo->region_stRect_x4);
    cJSON_AddNumberToObject(cjsonAiAttr, "region_y4",  pstPdInfo->region_stRect_y4);
    cJSON_AddNumberToObject(cjsonAiAttr, "region_x5",  pstPdInfo->region_stRect_x5);
    cJSON_AddNumberToObject(cjsonAiAttr, "region_y5",  pstPdInfo->region_stRect_y5);
    cJSON_AddNumberToObject(cjsonAiAttr, "region_x6",  pstPdInfo->region_stRect_x6);
    cJSON_AddNumberToObject(cjsonAiAttr, "region_y6",  pstPdInfo->region_stRect_y6);

    //cry
#if defined AUDIO_SUPPORT && defined AI_BABYCRY_SUPPORT
    cJSON_AddNumberToObject(cjsonAiAttr, "ai_model", 21);
    cJSON_AddNumberToObject(cjsonAiAttr, "cry_enable", app_ipcam_Ai_Cry_StatusGet());  
#endif
    str = cJSON_Print(cjsonAiAttr);
    if (str) {
        CVI_NET_AddCgiResponse(param, "%s", str);
        cJSON_free(str);
    }
    cJSON_Delete(cjsonAiAttr);

    return 0;
}

static int SetAiInfoCallBack(void *param, const char *cmd, const char *val)
{
    printf("enter: %s, %s %s\n", __func__, cmd, val);
    char decode[1024] = {0};
    cJSON *cjsonParser = NULL;
    cJSON *cjsonObj = NULL;
    int ret = 0;

    APP_PD_INFO_S PdInfo = {0};
    APP_MD_INFO_S MdInfo = {0};
#if defined AUDIO_SUPPORT && defined AI_BABYCRY_SUPPORT
    APP_CRY_INFO_S CryInfo = {0};
#endif
    UrlDecode(val, decode);
    printf("%s\n", decode);

    cjsonParser = cJSON_Parse(decode);
    if(cjsonParser == NULL) {
        printf("parse fail.\n");
        return -1;
    }
    //md
    cjsonObj = cJSON_GetObjectItem(cjsonParser, "md_enable");
    _NULL_POINTER_CHECK_(cjsonObj->valuestring, -1);
    MdInfo.enabled = atoi(cjsonObj->valuestring);

    cjsonObj = cJSON_GetObjectItem(cjsonParser, "md_threshold");
    _NULL_POINTER_CHECK_(cjsonObj->valuestring, -1);
    MdInfo.threshold = atoi(cjsonObj->valuestring);
    //cry
#if defined AUDIO_SUPPORT && defined AI_BABYCRY_SUPPORT
    cjsonObj = cJSON_GetObjectItem(cjsonParser, "cry_enable");
    _NULL_POINTER_CHECK_(cjsonObj->valuestring, -1);
    CryInfo.enabled = atoi(cjsonObj->valuestring); 
#endif  
    //pd
    cjsonObj = cJSON_GetObjectItem(cjsonParser, "pd_enable");
    _NULL_POINTER_CHECK_(cjsonObj->valuestring, -1);
    PdInfo.enabled = atoi(cjsonObj->valuestring);
  
    cjsonObj = cJSON_GetObjectItem(cjsonParser, "pd_threshold");
    _NULL_POINTER_CHECK_(cjsonObj->valuestring, -1);
    PdInfo.threshold = atoi(cjsonObj->valuestring) / 100.0;

    cjsonObj = cJSON_GetObjectItem(cjsonParser, "pd_intrusion_enable");
    _NULL_POINTER_CHECK_(cjsonObj->valuestring, -1);
    PdInfo.Intrusion_enabled = atoi(cjsonObj->valuestring);

    cjsonObj = cJSON_GetObjectItem(cjsonParser, "region_x1");
    _NULL_POINTER_CHECK_(cjsonObj->valuestring, -1);
    PdInfo.region_stRect_x1 = atoi(cjsonObj->valuestring);

    cjsonObj = cJSON_GetObjectItem(cjsonParser, "region_y1");
    _NULL_POINTER_CHECK_(cjsonObj->valuestring, -1);
    PdInfo.region_stRect_y1 = atoi(cjsonObj->valuestring);

    cjsonObj = cJSON_GetObjectItem(cjsonParser, "region_x2");
    _NULL_POINTER_CHECK_(cjsonObj->valuestring, -1);
    PdInfo.region_stRect_x2 = atoi(cjsonObj->valuestring);

    cjsonObj = cJSON_GetObjectItem(cjsonParser, "region_y2");
    _NULL_POINTER_CHECK_(cjsonObj->valuestring, -1);
    PdInfo.region_stRect_y2 = atoi(cjsonObj->valuestring);

    cjsonObj = cJSON_GetObjectItem(cjsonParser, "region_x3");
    _NULL_POINTER_CHECK_(cjsonObj->valuestring, -1);
    PdInfo.region_stRect_x3 = atoi(cjsonObj->valuestring);

    cjsonObj = cJSON_GetObjectItem(cjsonParser, "region_y3");
    _NULL_POINTER_CHECK_(cjsonObj->valuestring, -1);
    PdInfo.region_stRect_y3 = atoi(cjsonObj->valuestring);

    cjsonObj = cJSON_GetObjectItem(cjsonParser, "region_x4");
    _NULL_POINTER_CHECK_(cjsonObj->valuestring, -1);
    PdInfo.region_stRect_x4 = atoi(cjsonObj->valuestring);

    cjsonObj = cJSON_GetObjectItem(cjsonParser, "region_y4");
    _NULL_POINTER_CHECK_(cjsonObj->valuestring, -1);
    PdInfo.region_stRect_y4 = atoi(cjsonObj->valuestring);

    cjsonObj = cJSON_GetObjectItem(cjsonParser, "region_x5");
    _NULL_POINTER_CHECK_(cjsonObj->valuestring, -1);
    PdInfo.region_stRect_x5 = atoi(cjsonObj->valuestring);

    cjsonObj = cJSON_GetObjectItem(cjsonParser, "region_y5");
    _NULL_POINTER_CHECK_(cjsonObj->valuestring, -1);
    PdInfo.region_stRect_y5 = atoi(cjsonObj->valuestring);

    cjsonObj = cJSON_GetObjectItem(cjsonParser, "region_x6");
    _NULL_POINTER_CHECK_(cjsonObj->valuestring, -1);
    PdInfo.region_stRect_x6 = atoi(cjsonObj->valuestring);

    cjsonObj = cJSON_GetObjectItem(cjsonParser, "region_y6");
    _NULL_POINTER_CHECK_(cjsonObj->valuestring, -1);
    PdInfo.region_stRect_y6 = atoi(cjsonObj->valuestring);
  
    ret = CVI_IPC_NetCtrlSetMd(MdInfo);
#if defined AUDIO_SUPPORT && defined AI_BABYCRY_SUPPORT
    ret = CVI_IPC_NetCtrlSetCry(CryInfo);
#endif
    ret = CVI_IPC_NetCtrlSetPd(PdInfo);
   
    CVI_NET_AddCgiResponse(param, "ret: %d\n", ret);
    cJSON_Delete(cjsonParser);

    return ret;
}

static int UnRegiserAiFaceCallBack(void *param, const char *cmd, const char *val)
{
#ifdef IR_FACE_SUPPORT
    int ret = 0;
    char decode[1024] = {0};
    cJSON *cjsonParser = NULL;
    cJSON *cjsonObj = NULL;
    printf("enter: %s, %s %s\n", __func__, cmd, val);
    UrlDecode(val, decode);
    printf("%s\n", decode);
    cjsonParser = cJSON_Parse(decode);
    if(cjsonParser == NULL) {
        printf("parse fail.\n");
        return -1;
    }
    cjsonObj = cJSON_GetObjectItem(cjsonParser, "ai_name");
    char * ai_name = NULL;
    ai_name = cjsonObj->valuestring;
    ret = app_ipcam_Ai_IR_FD_UnRegister(ai_name);
    CVI_NET_AddCgiResponse(param, "ret: %d\n", ret);
    cJSON_Delete(cjsonParser);
#endif
    return 0;
}

static int RegiserAiFaceCallBack(void *param, const char *cmd, const char *val)
{
#ifdef IR_FACE_SUPPORT
    int ret = 0;
    char decode[1024] = {0};
    cJSON *cjsonParser = NULL;
    cJSON *cjsonObj = NULL;
    char * ai_name = NULL;
    printf("enter: %s, %s %s\n", __func__, cmd, val);
    UrlDecode(val, decode);
    printf("%s\n", decode);
    cjsonParser = cJSON_Parse(decode);
    if(cjsonParser == NULL) {
        printf("parse fail.\n");
        return -1;
    }
    cjsonObj = cJSON_GetObjectItem(cjsonParser, "ai_name");
    ai_name = cjsonObj->valuestring;
    ret = app_ipcam_Ai_IR_FD_Register(ai_name);
    CVI_NET_AddCgiResponse(param, "ret: %d\n", ret);
    cJSON_Delete(cjsonParser);
#endif
    return 0;
}

#endif
/*
*   video page end
*/

/*
*   osd page start
*/
#ifdef OSD_SUPPORT
static int GetOsdInfoCallBack(void *param, const char *cmd, const char *val)
{
    cJSON* cjsonOsdAttr = NULL;
    char* str = NULL;

    APP_OSDC_OBJS_INFO_S *pstOsdcPrivacy = app_ipcam_OsdcPrivacy_Param_Get();

    cjsonOsdAttr = cJSON_CreateObject();

    printf("enter: %s\n", __func__);


    APP_PARAM_OSDC_CFG_S *pstOsdcCfg = app_ipcam_Osdc_Param_Get();
    cJSON_AddNumberToObject(cjsonOsdAttr, "osd_global", pstOsdcCfg->enable);
    printf("\n");
    // timestamp
    cJSON_AddNumberToObject(cjsonOsdAttr, "osd_time", pstOsdcCfg->osdcObj[0][OSD_TIME_INDEX].bShow);
    cJSON_AddNumberToObject(cjsonOsdAttr, "osd_ts_x", pstOsdcCfg->osdcObj[0][OSD_TIME_INDEX].x1);
    cJSON_AddNumberToObject(cjsonOsdAttr, "osd_ts_y", pstOsdcCfg->osdcObj[0][OSD_TIME_INDEX].y1);
    // text1
    cJSON_AddNumberToObject(cjsonOsdAttr, "osd_text1", pstOsdcCfg->osdcObj[0][OSD_TEXT1_INDEX].bShow);
    cJSON_AddNumberToObject(cjsonOsdAttr, "osd_text1_x", pstOsdcCfg->osdcObj[0][OSD_TEXT1_INDEX].x1);
    cJSON_AddNumberToObject(cjsonOsdAttr, "osd_text1_y", pstOsdcCfg->osdcObj[0][OSD_TEXT1_INDEX].y1);
    cJSON_AddStringToObject(cjsonOsdAttr, "osd_text1_content", pstOsdcCfg->osdcObj[0][OSD_TEXT1_INDEX].str);
    cJSON_AddNumberToObject(cjsonOsdAttr, "osd_text1_color", pstOsdcCfg->osdcObj[0][OSD_TEXT1_INDEX].color);
    // text2
    cJSON_AddNumberToObject(cjsonOsdAttr, "osd_text2", pstOsdcCfg->osdcObj[1][OSD_TEXT2_INDEX].bShow);
    cJSON_AddNumberToObject(cjsonOsdAttr, "osd_text2_x", pstOsdcCfg->osdcObj[1][OSD_TEXT2_INDEX].x1);
    cJSON_AddNumberToObject(cjsonOsdAttr, "osd_text2_y", pstOsdcCfg->osdcObj[1][OSD_TEXT2_INDEX].y1);
    cJSON_AddStringToObject(cjsonOsdAttr, "osd_text2_content", pstOsdcCfg->osdcObj[1][OSD_TEXT2_INDEX].str);
    cJSON_AddNumberToObject(cjsonOsdAttr, "osd_text2_color", pstOsdcCfg->osdcObj[1][OSD_TEXT2_INDEX].color);
    // text3
    cJSON_AddNumberToObject(cjsonOsdAttr, "osd_text3", pstOsdcCfg->osdcObj[0][OSD_TEXT3_INDEX].bShow);
    cJSON_AddNumberToObject(cjsonOsdAttr, "osd_text3_x", pstOsdcCfg->osdcObj[0][OSD_TEXT3_INDEX].x1);
    cJSON_AddNumberToObject(cjsonOsdAttr, "osd_text3_y", pstOsdcCfg->osdcObj[0][OSD_TEXT3_INDEX].y1);
    cJSON_AddStringToObject(cjsonOsdAttr, "osd_text3_content1", pstOsdcCfg->osdcObj[0][OSD_TEXT3_INDEX].str);
    cJSON_AddNumberToObject(cjsonOsdAttr, "osd_text3_color", pstOsdcCfg->osdcObj[0][OSD_TEXT3_INDEX].color);
    // privacy area
    CVI_U32 i = 0;
    for (i = 0; i < pstOsdcCfg->osdcObjNum[0]; i++) {
        if ((pstOsdcCfg->osdcObj[0][i].filled == CVI_TRUE) &&
            (pstOsdcCfg->osdcObj[0][i].type == RGN_CMPR_RECT)) {
                cJSON_AddNumberToObject(cjsonOsdAttr, "osd_privacy", pstOsdcCfg->osdcObj[0][i].bShow);
                cJSON_AddNumberToObject(cjsonOsdAttr, "osd_privacy_x", pstOsdcCfg->osdcObj[0][i].x1);
                cJSON_AddNumberToObject(cjsonOsdAttr, "osd_privacy_y", pstOsdcCfg->osdcObj[0][i].y1);
                cJSON_AddNumberToObject(cjsonOsdAttr, "osd_privacy_width", pstOsdcCfg->osdcObj[0][i].width);
                cJSON_AddNumberToObject(cjsonOsdAttr, "osd_privacy_hight", pstOsdcCfg->osdcObj[0][i].height);
                cJSON_AddNumberToObject(cjsonOsdAttr, "osd_privacy_color", pstOsdcPrivacy->color);
                break;
        }
    }
    str = cJSON_Print(cjsonOsdAttr);
    if (str) {
        CVI_NET_AddCgiResponse(param, "%s", str);
        cJSON_free(str);
    }
    cJSON_Delete(cjsonOsdAttr);

    return 0;
}


static int SetOsdInfoCallBack(void *param, const char *cmd, const char *val)
{
    printf("enter: %s, %s %s\n", __func__, cmd, val);
    char decode[1024] = {0};
    cJSON *cjsonParser = NULL;
    cJSON *cjsonObj = NULL;
    int ret = 0;

    printf("%zu %zu\n", strlen(cmd), strlen(val));
    UrlDecode(val, decode);
    printf("%s\n", decode);

    cjsonParser = cJSON_Parse(decode);
    if(cjsonParser == NULL) {
        printf("parse fail.\n");
        return -1;
    }
    APP_PARAM_OSDC_CFG_S *pstOsdcCfg = app_ipcam_Osdc_Param_Get();
    APP_OSDC_OBJS_INFO_S *pstOsdcPrivacy = app_ipcam_OsdcPrivacy_Param_Get();
    APP_PARAM_OSDC_CFG_S stOsdcCfg;
    memcpy(&stOsdcCfg, pstOsdcCfg, sizeof(APP_PARAM_OSDC_CFG_S));
    cjsonObj = cJSON_GetObjectItem(cjsonParser, "global_switch");
    _NULL_POINTER_CHECK_(cjsonObj->valuestring, -1);
    stOsdcCfg.enable = atoi(cjsonObj->valuestring);
    // timestamp
    cjsonObj = cJSON_GetObjectItem(cjsonParser, "ts_switch");
    _NULL_POINTER_CHECK_(cjsonObj->valuestring, -1);
    stOsdcCfg.osdcObj[0][OSD_TIME_INDEX].bShow = atoi(cjsonObj->valuestring);
    cjsonObj = cJSON_GetObjectItem(cjsonParser, "ts_x");
    _NULL_POINTER_CHECK_(cjsonObj->valuestring, -1);
    stOsdcCfg.osdcObj[0][OSD_TIME_INDEX].x1 = atoi(cjsonObj->valuestring);
    cjsonObj = cJSON_GetObjectItem(cjsonParser, "ts_y");
    _NULL_POINTER_CHECK_(cjsonObj->valuestring, -1);
    stOsdcCfg.osdcObj[0][OSD_TIME_INDEX].y1 = atoi(cjsonObj->valuestring);
    // text1
    cjsonObj = cJSON_GetObjectItem(cjsonParser, "text1_switch");
    _NULL_POINTER_CHECK_(cjsonObj->valuestring, -1);
    stOsdcCfg.osdcObj[0][OSD_TEXT1_INDEX].bShow = atoi(cjsonObj->valuestring);
    cjsonObj = cJSON_GetObjectItem(cjsonParser, "text1_x");
    _NULL_POINTER_CHECK_(cjsonObj->valuestring, -1);
    stOsdcCfg.osdcObj[0][OSD_TEXT1_INDEX].x1 = atoi(cjsonObj->valuestring);
    cjsonObj = cJSON_GetObjectItem(cjsonParser, "text1_y");
    _NULL_POINTER_CHECK_(cjsonObj->valuestring, -1);
    stOsdcCfg.osdcObj[0][OSD_TEXT1_INDEX].y1 = atoi(cjsonObj->valuestring);
    cjsonObj = cJSON_GetObjectItem(cjsonParser, "text1_content");
    _NULL_POINTER_CHECK_(cjsonObj->valuestring, -1);
    if (!(strcmp(cjsonObj->valuestring, "") == 0))
    {
        memcpy(stOsdcCfg.osdcObj[0][OSD_TEXT1_INDEX].str, cjsonObj->valuestring, APP_OSD_STR_LEN_MAX);
    }
    else
    {
        memcpy(stOsdcCfg.osdcObj[0][OSD_TEXT1_INDEX].str, " ", 2);
    }

    // text2
    //because conflict with pd instruction rect
    if (stOsdcCfg.osdcObj[1][OSD_TEXT2_INDEX].type == RGN_CMPR_BIT_MAP) {
        cjsonObj = cJSON_GetObjectItem(cjsonParser, "text2_switch");
        _NULL_POINTER_CHECK_(cjsonObj->valuestring, -1);
        stOsdcCfg.osdcObj[1][OSD_TEXT2_INDEX].bShow = atoi(cjsonObj->valuestring);
        cjsonObj = cJSON_GetObjectItem(cjsonParser, "text2_x");
        _NULL_POINTER_CHECK_(cjsonObj->valuestring, -1);
        stOsdcCfg.osdcObj[1][OSD_TEXT2_INDEX].x1 = atoi(cjsonObj->valuestring);
        cjsonObj = cJSON_GetObjectItem(cjsonParser, "text2_y");
        _NULL_POINTER_CHECK_(cjsonObj->valuestring, -1);
        stOsdcCfg.osdcObj[1][OSD_TEXT2_INDEX].y1 = atoi(cjsonObj->valuestring);
        cjsonObj = cJSON_GetObjectItem(cjsonParser, "text2_content");
        _NULL_POINTER_CHECK_(cjsonObj->valuestring, -1);
        if (!(strcmp(cjsonObj->valuestring, "") == 0))
        {
            memcpy(stOsdcCfg.osdcObj[1][OSD_TEXT2_INDEX].str, cjsonObj->valuestring, APP_OSD_STR_LEN_MAX);
        }
        else
        {
            memcpy(stOsdcCfg.osdcObj[1][OSD_TEXT2_INDEX].str, " ", 2);
        }
    }

    // text3
    //because conflict with pd instruction rect
    if (stOsdcCfg.osdcObj[0][OSD_TEXT3_INDEX].type == RGN_CMPR_BIT_MAP) {
        cjsonObj = cJSON_GetObjectItem(cjsonParser, "text3_switch");
        _NULL_POINTER_CHECK_(cjsonObj->valuestring, -1);
        stOsdcCfg.osdcObj[0][OSD_TEXT3_INDEX].bShow = atoi(cjsonObj->valuestring);
                                        
        cjsonObj = cJSON_GetObjectItem(cjsonParser, "text3_x");
        _NULL_POINTER_CHECK_(cjsonObj->valuestring, -1);
        stOsdcCfg.osdcObj[0][OSD_TEXT3_INDEX].x1 = atoi(cjsonObj->valuestring);
                                        
        cjsonObj = cJSON_GetObjectItem(cjsonParser, "text3_y");
        _NULL_POINTER_CHECK_(cjsonObj->valuestring, -1);
        stOsdcCfg.osdcObj[0][OSD_TEXT3_INDEX].y1 = atoi(cjsonObj->valuestring);

        cjsonObj = cJSON_GetObjectItem(cjsonParser, "text3_content1");
        _NULL_POINTER_CHECK_(cjsonObj->valuestring, -1);
        if (!(strcmp(cjsonObj->valuestring, "") == 0))
        {
            memcpy(stOsdcCfg.osdcObj[0][OSD_TEXT3_INDEX].str, cjsonObj->valuestring, APP_OSD_STR_LEN_MAX);
        }
        else
        {
            memcpy(stOsdcCfg.osdcObj[0][OSD_TEXT3_INDEX].str, " ", 2);
        }
        
    }
    // privacy area
    CVI_U32 i = 0;
    for (i = 0; i < stOsdcCfg.osdcObjNum[0]; i++) {
        if ((stOsdcCfg.osdcObj[0][i].filled == CVI_TRUE) &&
            (stOsdcCfg.osdcObj[0][i].type == RGN_CMPR_RECT)) {
            cjsonObj = cJSON_GetObjectItem(cjsonParser, "privacy_switch");
            _NULL_POINTER_CHECK_(cjsonObj->valuestring, -1);
            pstOsdcPrivacy->bShow= stOsdcCfg.osdcObj[0][i].bShow = atoi(cjsonObj->valuestring);
            cjsonObj = cJSON_GetObjectItem(cjsonParser, "privacy_x");
            _NULL_POINTER_CHECK_(cjsonObj->valuestring, -1);
            pstOsdcPrivacy->x1 = stOsdcCfg.osdcObj[0][i].x1 = atoi(cjsonObj->valuestring);
            cjsonObj = cJSON_GetObjectItem(cjsonParser, "privacy_y");
            _NULL_POINTER_CHECK_(cjsonObj->valuestring, -1);
            pstOsdcPrivacy->y1 = stOsdcCfg.osdcObj[0][i].y1 = atoi(cjsonObj->valuestring);
            cjsonObj = cJSON_GetObjectItem(cjsonParser, "privacy_width");
            _NULL_POINTER_CHECK_(cjsonObj->valuestring, -1);
            pstOsdcPrivacy->width = stOsdcCfg.osdcObj[0][i].width = atoi(cjsonObj->valuestring);
            cjsonObj = cJSON_GetObjectItem(cjsonParser, "privacy_hight");
            _NULL_POINTER_CHECK_(cjsonObj->valuestring, -1);
            pstOsdcPrivacy->height = stOsdcCfg.osdcObj[0][i].height = atoi(cjsonObj->valuestring);
            cjsonObj = cJSON_GetObjectItem(cjsonParser, "privacy_color");
            _NULL_POINTER_CHECK_(cjsonObj->valuestring, -1);
            IDX_TO_COLOR(0, atoi(cjsonObj->valuestring), stOsdcCfg.osdcObj[0][i].color);
            COLOR_TO_IDX(0, pstOsdcPrivacy->color, stOsdcCfg.osdcObj[0][i].color);
            break;
        }
    }

    if (0 != memcmp(&stOsdcCfg, pstOsdcCfg, sizeof(APP_PARAM_OSDC_CFG_S))) {
        app_ipcam_Osdc_Status(&stOsdcCfg);
    }

    CVI_NET_AddCgiResponse(param, "ret: %d\n", ret);

    cJSON_Delete(cjsonParser);
    return 0;
}
#endif
/*
*   osd page end
*/

/*
*   audio page start
*/
#ifdef AUDIO_SUPPORT
static int GetAudioInfoCallBack(void *param, const char *cmd, const char *val)
{
    cJSON* cjsonAudioAttr = NULL;
    char* str = NULL;

    APP_PARAM_AUDIO_CFG_T *pstAudioCfg = app_ipcam_Audio_Param_Get();

    cjsonAudioAttr = cJSON_CreateObject();

    printf("enter: %s\n", __func__);

    cJSON_AddNumberToObject(cjsonAudioAttr, "main_enabled", pstAudioCfg->astAudioIntercom.bEnable);

    cJSON_AddStringToObject(cjsonAudioAttr, "ip", pstAudioCfg->astAudioIntercom.cIp);

    char tmpStr[16];
    memset(tmpStr, 0, sizeof(tmpStr));
    snprintf(tmpStr, sizeof(tmpStr), "%d", pstAudioCfg->astAudioIntercom.iPort);
    cJSON_AddStringToObject(cjsonAudioAttr, "port", tmpStr);

    cJSON_AddNumberToObject(cjsonAudioAttr, "in_slider", pstAudioCfg->astAudioVol.iAdcLVol);
    cJSON_AddNumberToObject(cjsonAudioAttr, "out_slider", pstAudioCfg->astAudioVol.iDacRVol);

    cJSON_AddNumberToObject(cjsonAudioAttr, "amplifier_enable", pstAudioCfg->bInit);

    cJSON_AddNumberToObject(cjsonAudioAttr, "codec_select", pstAudioCfg->astAudioCfg.enAencType);
    cJSON_AddNumberToObject(cjsonAudioAttr, "samplerate_select", pstAudioCfg->astAudioCfg.enSamplerate);
    cJSON_AddNumberToObject(cjsonAudioAttr, "anr", pstAudioCfg->astAudioVqe.bAiAnrEnable);
    cJSON_AddNumberToObject(cjsonAudioAttr, "agc", pstAudioCfg->astAudioVqe.bAiAgcEnable);

    str = cJSON_Print(cjsonAudioAttr);
    if (str) {
        CVI_NET_AddCgiResponse(param, "%s", str);
        cJSON_free(str);
    }
    cJSON_Delete(cjsonAudioAttr);

    return 0;
}

static int SetAudioInfoCallBack(void *param, const char *cmd, const char *val)
{
    printf("enter: %s [%s|%s]\n", __func__, cmd, val);
    int ret = 0;
    char tmpStr[64];
    char decode[1024] = {0};
    APP_PARAM_AUDIO_CFG_T stAudioCfg;
    cJSON *cjsonParser = NULL;
    cJSON *cjsonObj = NULL;

    UrlDecode(val, decode);
    printf("%s\n", decode);

    cjsonParser = cJSON_Parse(decode);
    if(cjsonParser == NULL) {
        printf("parse fail.\n");
        return -1;
    }
    memcpy(&stAudioCfg, app_ipcam_Audio_Param_Get(), sizeof(APP_PARAM_AUDIO_CFG_T));
    cjsonObj = cJSON_GetObjectItem(cjsonParser, "main_enabled");
    _NULL_POINTER_CHECK_(cjsonObj->valuestring, -1);
    stAudioCfg.astAudioIntercom.bEnable = atoi(cjsonObj->valuestring);

    cjsonObj = cJSON_GetObjectItem(cjsonParser, "ip");
    _NULL_POINTER_CHECK_(cjsonObj->valuestring, -1);
    memcpy(stAudioCfg.astAudioIntercom.cIp, cjsonObj->valuestring, strlen(cjsonObj->valuestring) + 1);
    cjsonObj = cJSON_GetObjectItem(cjsonParser, "port");
    _NULL_POINTER_CHECK_(cjsonObj->valuestring, -1);
    memcpy(tmpStr, cjsonObj->valuestring, strlen(cjsonObj->valuestring) + 1);
    stAudioCfg.astAudioIntercom.iPort = atoi(tmpStr);
    cjsonObj = cJSON_GetObjectItem(cjsonParser, "in_slider");
    _NULL_POINTER_CHECK_(cjsonObj->valuestring, -1);
    stAudioCfg.astAudioVol.iAdcLVol = atoi(cjsonObj->valuestring);
    cjsonObj = cJSON_GetObjectItem(cjsonParser, "out_slider");
    _NULL_POINTER_CHECK_(cjsonObj->valuestring, -1);
    stAudioCfg.astAudioVol.iDacRVol = atoi(cjsonObj->valuestring);
    cjsonObj = cJSON_GetObjectItem(cjsonParser, "amplifier_enable");
    _NULL_POINTER_CHECK_(cjsonObj->valuestring, -1);
    stAudioCfg.bInit = atoi(cjsonObj->valuestring);
    cjsonObj = cJSON_GetObjectItem(cjsonParser, "codec_select");
    _NULL_POINTER_CHECK_(cjsonObj->valuestring, -1);
    stAudioCfg.astAudioCfg.enAencType = atoi(cjsonObj->valuestring);
    cjsonObj = cJSON_GetObjectItem(cjsonParser, "samplerate_select");
    _NULL_POINTER_CHECK_(cjsonObj->valuestring, -1);
    stAudioCfg.astAudioCfg.enSamplerate = atoi(cjsonObj->valuestring);
    cjsonObj = cJSON_GetObjectItem(cjsonParser, "anr");
    _NULL_POINTER_CHECK_(cjsonObj->valuestring, -1);
    stAudioCfg.astAudioVqe.bAiAnrEnable = atoi(cjsonObj->valuestring);
    cjsonObj = cJSON_GetObjectItem(cjsonParser, "agc");
    _NULL_POINTER_CHECK_(cjsonObj->valuestring, -1);
    stAudioCfg.astAudioVqe.bAiAgcEnable = atoi(cjsonObj->valuestring);

    if (stAudioCfg.astAudioIntercom.bEnable)
    {
        stAudioCfg.astAudioCfg.u32ChnCnt = 2;
        stAudioCfg.astAudioCfg.enSoundmode = AUDIO_SOUND_MODE_STEREO;
        stAudioCfg.astAudioVqe.bAiAecEnable = 1;
    }
    else
    {
        stAudioCfg.astAudioCfg.enSoundmode = AUDIO_SOUND_MODE_MONO;
        stAudioCfg.astAudioVqe.bAiAecEnable = 0;
    }
    ret = app_ipcam_Audio_AudioReset(&stAudioCfg);

    CVI_NET_AddCgiResponse(param, "ret: %d\n", ret);
    return 0;
}

static int RecordAudioInfoCallBack(void *param, const char *cmd, const char *val)
{
    printf("enter: %s [%s|%s]\n", __func__, cmd, val);
    int ret = 0;
    APP_AUDIO_RECORD_T stAudioRecord;

    memset(&stAudioRecord, 0, sizeof(APP_AUDIO_RECORD_T));
    if (strstr(val, "in_name") != NULL) {
        NetString2Str(val, stAudioRecord.cAencFileName);
        stAudioRecord.iStatus = 1;
        app_ipcam_Audio_SetRecordStatus(&stAudioRecord);
    } else if (strstr(val, "out_name") != NULL) {
        NetString2Str(val, stAudioRecord.cAencFileName);
        stAudioRecord.iStatus = 1;
        app_ipcam_Audio_SetPlayStatus(&stAudioRecord);
    } else if (strstr(val, "in_stop") != NULL) {
        app_ipcam_Audio_SetRecordStatus(&stAudioRecord);
    } else if (strstr(val, "out_stop") != NULL) {
        app_ipcam_Audio_SetPlayStatus(&stAudioRecord);
    } else {
        printf("%s %d: error no support setting, %s %s\n", __func__, __LINE__, cmd, val);
    }
    CVI_NET_AddCgiResponse(param, "ret: %d\n", ret);

    return ret;
}
#endif
/*
*   audio page end
*/

/*
*   OTA page start
*/
static int OTAGetStatusCallBack(void *param , const char *cmd , const char *val)
{
    printf("enter: %s [%s|%s]\n", __func__, cmd, val);
    char buf[512] = {0};
    int ret = app_ipcam_OTA_GetUpgradeStatus(buf , sizeof(buf));

    CVI_NET_AddCgiResponse(param, "%s\n", buf);
    return ret;
}

static int OTAGetVersionCallBack(void *param , const char *cmd , const char *val)
{
    printf("enter: %s [%s|%s]\n", __func__, cmd, val);
    char buf[512] = {0};
    int ret = app_ipcam_OTA_GetSystemVersion(buf , sizeof(buf));
    CVI_NET_AddCgiResponse(param, "%s\n", buf);
    return ret;
}

static int OTAUploadCallBack(void *param , const char *cmd , const char *val)
{
    printf("enter: %s [%s|%s]\n", __func__, cmd, val);

    pthread_t otaUploadID;
    char buf[512] = "<hr>OTA Upload Start...\n";
    CVI_NET_AddCgiResponse(param, "%s\n", buf);
    if(app_ipcam_OTA_UploadFW(param) != NULL)
    {
        app_ipcam_OTA_CloseThreadBeforeUpgrade();
        pthread_create(&otaUploadID , NULL , app_ipcam_OTA_UnpackAndUpdate , NULL);
        memcpy(buf , "<hr>OTA Upgrade and Reboot..\n" , sizeof("<hr>OTA Upgrade and Reboot..\n"));
    }
    else
        memcpy(buf , "<hr>OTA Upgrade failed!\n" , sizeof("<hr>OTA Upgrade failed!\n"));
    CVI_NET_AddCgiResponse(param, "%s\n", buf);

    return 0;
}
/*
*   OTA page end
*/

/*
*  preview page get/set CB list
*/
static int app_ipcam_IcgiRegister_Preview(void)
{
    printf("enter: %s\n", __func__);
    IcgiRegister("get_ws_addr.cgi", NULL, (void *)GetWsAddrCallBack);
    IcgiRegister("get_cur_chn.cgi", NULL, (void *)GetCurChnCallBack);
    IcgiRegister("switch_stream.cgi", "chn", (void *)SetCurChnCallBack);
    IcgiRegister("/takePhoto", NULL, (void *)TakePhotoCallBack);
    return 0;
}
/*
*  image page get/set CB list
*/
static int app_ipcam_IcgiRegister_Image(void)
{
    printf("enter: %s\n", __func__);
    IcgiRegister("get_img_info.cgi", NULL, (void *)GetImgInfoCallBack);
    IcgiRegister("set_img_info.cgi", NULL, (void *)SetImgInfoCallBack);
    return 0;
}
/*
*  video page get/set CB list
*/
static int app_ipcam_IcgiRegister_Video(void)
{
    printf("enter: %s\n", __func__);
    IcgiRegister("get_stream_cfg.cgi", NULL, (void *)GetStreamCfgCallBack);
    IcgiRegister("set_stream_cfg.cgi", NULL, (void *)SetStreamCfgCallBack);
    IcgiRegister("set_roi_cfg.cgi", NULL, (void *)SetRoiCfgCallBack);
    IcgiRegister("get_roi_cfg.cgi", NULL, (void *)GetRoiCfgCallBack);
#ifdef RECORD_SUPPORT
    //IcgiRegister("set_record_sectret.cgi", NULL, (void *)SetRecordCallBack);
    //IcgiRegister("get_record_sectret.cgi", NULL, (void *)GetRecordCallBack);
    //IcgiRegister("start_replay_sectret.cgi", NULL, (void *)StartRepalyCallBack);
    //IcgiRegister("stop_replay_sectret.cgi", NULL, (void *)StopRepalyCallBack);
#endif
    return 0;
}

#ifdef OSD_SUPPORT
/*
*  OSD page get/set CB list
*/
static int app_ipcam_IcgiRegister_Osd(void)
{
    printf("enter: %s\n", __func__);
    IcgiRegister("get_osd_info.cgi", NULL, (void *)GetOsdInfoCallBack);
    IcgiRegister("set_osd_info.cgi", NULL, (void *)SetOsdInfoCallBack);
    return 0;
}
#endif

/*
*  AUDIO page get/set CB list
*/
#if AUDIO_SUPPORT
static int app_ipcam_IcgiRegister_Audio(void)
{
    printf("enter: %s\n", __func__);
    IcgiRegister("get_audio_cfg.cgi", NULL, (void *)GetAudioInfoCallBack);
    IcgiRegister("set_audio_cfg.cgi", NULL, (void *)SetAudioInfoCallBack);
    IcgiRegister("audio_record_sectret.cgi", NULL, (void *)RecordAudioInfoCallBack);
    return 0;
}
#endif

/*
*  AI page get/set CB list
*/
#ifdef AI_SUPPORT
static int app_ipcam_IcgiRegister_Ai(void)
{
    printf("enter: %s\n", __func__);
    IcgiRegister("get_ai_info.cgi", NULL, (void *)GetAiInfoCallBack);
    IcgiRegister("set_ai_info.cgi", NULL, (void *)SetAiInfoCallBack);
    IcgiRegister("register_ai_face.cgi", NULL, (void *)RegiserAiFaceCallBack);
    IcgiRegister("unregister_ai_face.cgi", NULL, (void *)UnRegiserAiFaceCallBack);
    return 0;
}
#endif

/*
*  OTA page get/upload CB list
*/
static int app_ipcam_IcgiRegister_OTA()
{
    printf("enter: %s\n", __func__);
    IcgiRegister("getotastatus.cgi", NULL, (void *)OTAGetStatusCallBack);
    IcgiRegister("getversion.cgi", NULL, (void *)OTAGetVersionCallBack);
    IcgiRegister("upload.cgi", NULL, (void *)OTAUploadCallBack);
    return 0;
}

int app_ipcam_NetCtrl_Init()
{
    printf("app_ipcam_NetCtrl_Init\n");
    char path[] = "/mnt/sd/www";
    CVI_NET_SetVideoPath(path);

    app_ipcam_IcgiRegister_Preview();
    app_ipcam_IcgiRegister_Image();
    app_ipcam_IcgiRegister_Video();
#ifdef OSD_SUPPORT
    app_ipcam_IcgiRegister_Osd();
#endif
#ifdef AUDIO_SUPPORT
    app_ipcam_IcgiRegister_Audio();
#endif
#ifdef AI_SUPPORT
    app_ipcam_IcgiRegister_Ai();
#endif
    app_ipcam_IcgiRegister_OTA();
    CVI_NET_Init();

    return 0;
}

int app_ipcam_NetCtrl_DeInit()
{
    CVI_NET_Deinit();

    return 0;
}
