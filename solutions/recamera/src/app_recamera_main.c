#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include "app_ipcam_paramparse.h"
#include "app_ipcam_ircut.h"

#ifdef WEB_SOCKET
#include "app_ipcam_websocket.h"
#include "app_ipcam_netctrl.h"
#endif
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
static pthread_t g_pthMisc;
static CVI_BOOL g_bMisc;

/**************************************************************************
 *                 E X T E R N A L    R E F E R E N C E S                 *
 **************************************************************************/

/**************************************************************************
 *               F U N C T I O N    D E C L A R A T I O N S               *
 **************************************************************************/
static int app_ipcam_Exit(void);

static CVI_VOID app_ipcam_ExitSig_handle(CVI_S32 signo)
{
    signal(SIGINT, SIG_IGN);
    signal(SIGTERM, SIG_IGN);
    
    if ((SIGINT == signo) || (SIGTERM == signo)) {
        app_ipcam_Exit();
        APP_PROF_LOG_PRINT(LEVEL_INFO, "ipcam receive a signal(%d) from terminate\n", signo);
    }

    exit(-1);
}

static CVI_VOID app_ipcam_Usr1Sig_handle(CVI_S32 signo)
{    
    if (SIGUSR1 == signo) {
        APP_PROF_LOG_PRINT(LEVEL_INFO, "ipcam receive a signal(%d) from terminate and start trigger a picture\n", signo);
        app_ipcam_JpgCapFlag_Set(CVI_TRUE);
    }
}

static int app_ipcam_Peripheral_Init(void)
{
    /* do peripheral Init here */

    app_ipcam_IRCut_Init();

    return CVI_SUCCESS;
}

/* 
* this thread handle a series of small tasks include
* a. send AI framerate to Web-client
* b. 
*/
static void *ThreadMisc(void *arg)
{
    while (g_bMisc) {
        #ifdef WEB_SOCKET
        app_ipcam_WebSocket_AiFps_Send();
        #endif
        sleep(1);
    }

    return NULL;
}

static int app_ipcam_MiscThread_Init(void)
{
    int s32Ret = CVI_SUCCESS;

    g_bMisc = CVI_TRUE;
    s32Ret = pthread_create(&g_pthMisc, NULL, ThreadMisc, NULL);
    if (s32Ret != 0) {
        printf("pthread_create failed!\n");
        return s32Ret;
    }

    return CVI_SUCCESS;
}

static int app_ipcam_MiscThread_DeInit(void)
{
    g_bMisc = CVI_FALSE;

    if (g_pthMisc) {
        pthread_cancel(g_pthMisc);
        pthread_join(g_pthMisc, NULL);
        g_pthMisc = 0;
    }

    return CVI_SUCCESS;
}

static int app_ipcam_Exit(void)
{
    APP_CHK_RET(app_ipcam_MiscThread_DeInit(), "DeInit Misc Process");

    #ifdef RECORD_SUPPORT
    APP_CHK_RET(app_ipcam_Record_UnInit(), "running SD Record");
    #endif

    APP_CHK_RET(app_ipcam_Osdc_DeInit(), "OsdC DeInit");

    #ifdef AI_SUPPORT
    APP_CHK_RET(app_ipcam_Ai_PD_Stop(), "PD Stop");

    APP_CHK_RET(app_ipcam_Ai_MD_Stop(), "MD Stop");

    #ifdef FACE_SUPPORT
    APP_CHK_RET(app_ipcam_Ai_FD_Stop(), "FD Stop");
    #endif
    #endif

    #ifdef AUDIO_SUPPORT
    APP_CHK_RET(app_ipcam_Audio_UnInit(), "Audio Stop");
    #endif

    APP_CHK_RET(app_ipcam_Vpss_DeInit(), "Vpss DeInit");

    APP_CHK_RET(app_ipcam_Venc_Stop(APP_VENC_ALL), "Venc Stop");

    APP_CHK_RET(app_ipcam_Vi_DeInit(), "Vi DeInit");

    APP_CHK_RET(app_ipcam_Sys_DeInit(), "System DeInit");

    APP_CHK_RET(app_ipcam_rtsp_Server_Destroy(), "RTSP Server Destroy");

    return CVI_SUCCESS;

}

static int app_ipcam_Init(void)
{

    APP_CHK_RET(app_ipcam_Peripheral_Init(), "init peripheral");

    APP_CHK_RET(app_ipcam_Sys_Init(), "init systerm");

    APP_CHK_RET(app_ipcam_Vi_Init(), "init vi module");

    APP_CHK_RET(app_ipcam_Vpss_Init(), "init vpss module");

    APP_CHK_RET(app_ipcam_Osdc_Init(), "init Draw Osdc");

    #ifdef WEB_SOCKET
    APP_CHK_RET(app_ipcam_NetCtrl_Init(), "Net Ctrl init");
    
    APP_CHK_RET(app_ipcam_WebSocket_Init(), "websocket init");
    #endif

    APP_CHK_RET(app_ipcam_Venc_Init(APP_VENC_ALL), "init video encode");

    #ifdef AUDIO_SUPPORT
    APP_CHK_RET(app_ipcam_Audio_Init(), "start audio processing");
    #endif

    APP_CHK_RET(app_ipcam_MiscThread_Init(), "Init Misc Process");

    return CVI_SUCCESS;
}

int main(int argc, char *argv[])
{
    system("echo /mnt/nfs/core-%e-%p-%t > /proc/sys/kernel/core_pattern");
    APP_CHK_RET(app_ipcam_Opts_Parse(argc, argv), "parse optinos");

    signal(SIGINT, app_ipcam_ExitSig_handle);
    signal(SIGTERM, app_ipcam_ExitSig_handle);
    signal(SIGUSR1, app_ipcam_Usr1Sig_handle);

    /* load each moudles parameter from param_config.ini */
    APP_CHK_RET(app_ipcam_Param_Load(), "load global parameter");

    /* init modules include <Peripheral; Sys; VI; VB; OSD; Venc; AI; Audio; etc.> */
    APP_CHK_RET(app_ipcam_Init(), "app_ipcam_Init");

    /* create rtsp server */
    APP_CHK_RET(app_ipcam_Rtsp_Server_Create(), "create rtsp server");

    /* start video encode */
    APP_CHK_RET(app_ipcam_Venc_Start(APP_VENC_ALL), "start video processing");

    #ifdef AI_SUPPORT
    /* start AI PD (Pedestrian Detection) */
    APP_CHK_RET(app_ipcam_Ai_PD_Start(), "running AI PD");

    /* start AI MD (Motion Detection)*/
    APP_CHK_RET(app_ipcam_Ai_MD_Start(), "running AI MD");

    /* start AI FD (Face Detection)*/
    #ifdef FACE_SUPPORT
    #ifdef IR_FACE_SUPPORT
    APP_CHK_RET(app_ipcam_Ai_IR_FD_Start(), "running AI IR FD");
    #else
    APP_CHK_RET(app_ipcam_Ai_FD_Start(), "running AI FD");
    #endif
    #endif

    #if defined AUDIO_SUPPORT && defined AI_BABYCRY_SUPPORT
    APP_CHK_RET(app_ipcam_Ai_Cry_Start(), "running AI CRY");
    #endif

    #endif

    #ifdef RECORD_SUPPORT
    /* start SD Record */
    app_ipcam_Record_Recover_Init();
    APP_CHK_RET(app_ipcam_Record_Init(), "running SD Record");
    #endif

    /* enable receive a command form another progress for test ipcam */
    //APP_CHK_RET(app_ipcam_CmdTask_Create(), "running cmd test");

    while (1) {
        sleep(1);
    };
    
    return CVI_SUCCESS;
}
