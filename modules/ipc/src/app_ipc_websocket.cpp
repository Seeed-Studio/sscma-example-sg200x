#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <net/if.h>
#include <net/route.h>
#include <netinet/in.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "libwebsockets.h"

#include "app_ipc_websocket.h"
#include "app_ipcam_comm.h"

#define MAX_PAYLOAD_SIZE (1024 * 1024)
#define MAX_BUFFER_SIZE (256 * 1024)

#define DEF_VENC_CHN 1

EXTERN_C_START()
static unsigned char* s_imgData = NULL;
static pthread_t g_websocketThread;
static volatile int s_terminal = 0;
pthread_mutex_t g_mutexLock = PTHREAD_MUTEX_INITIALIZER;
static int s_fileSize = 0;

struct sessionData_s {
    int msg_count;
    unsigned char buf[MAX_PAYLOAD_SIZE];
    int len;
    int bin;
    int fin;
};

struct lws* g_wsi = NULL;
static int ProtocolMyCallback(struct lws* wsi, enum lws_callback_reasons reason, void* user, void* in, size_t len)
{
    struct sessionData_s* data = (struct sessionData_s*)user;

    switch (reason) {
    case LWS_CALLBACK_ESTABLISHED:
        printf("LWS_CALLBACK_ESTABLISHED\n");
        g_wsi = wsi;
        CVI_VENC_RequestIDR(DEF_VENC_CHN, 1);
        break;

    case LWS_CALLBACK_RECEIVE:
        printf("LWS_CALLBACK_RECEIVE\n");
        break;

    case LWS_CALLBACK_SERVER_WRITEABLE:
        if (s_fileSize > 100000) {
            APP_PROF_LOG_PRINT(LEVEL_WARN, "client writeable s_fileSize:%d\n", s_fileSize);
        }
        pthread_mutex_lock(&g_mutexLock);
        if (s_fileSize != 0) {
            lws_write(wsi, s_imgData + LWS_PRE, s_fileSize, LWS_WRITE_BINARY);
            s_fileSize = 0;
        }
        pthread_mutex_unlock(&g_mutexLock);

        lws_rx_flow_control(wsi, 1);
        break;

    case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
        printf("LWS_CALLBACK_CLIENT_CONNECTION_ERROR\n");
        break;

    case LWS_CALLBACK_WSI_DESTROY:
        printf("LWS_CALLBACK_WSI_DESTROY\n");
    case LWS_CALLBACK_CLOSED:
        printf("LWS_CALLBACK_CLOSED\n");
        g_wsi = NULL;
        pthread_mutex_lock(&g_mutexLock);
        s_fileSize = 0;
        pthread_mutex_unlock(&g_mutexLock);
        break;

    default:
        break;
    }

    return 0;
}

struct lws_protocols protocols[] = {
    { "ws", /*协议名*/
        ProtocolMyCallback, /*回调函数*/
        sizeof(struct sessionData_s), /*定义新连接分配的内存,连接断开后释放*/
        MAX_PAYLOAD_SIZE, /*定义rx 缓冲区大小*/
        0,
        NULL,
        0 },
    { NULL, NULL, 0, 0, 0, 0, 0 } /*结束标志*/
};

static void* ThreadWebsocket(void* arg)
{
    struct lws_context_creation_info ctx_info = { 0 };
    ctx_info.port = 8000;
    ctx_info.iface = NULL; // 在所有网络接口上监听
    ctx_info.protocols = protocols;
    ctx_info.gid = -1;
    ctx_info.uid = -1;
    ctx_info.options = LWS_SERVER_OPTION_VALIDATE_UTF8;

    struct lws_context* context = lws_create_context(&ctx_info);
    while (!s_terminal) {
        lws_service(context, 1000);
    }
    lws_context_destroy(context);

    return NULL;
}

int app_ipc_WebSocket_Init(void)
{
    int s32Ret = 0;

    s_imgData = (unsigned char*)malloc(MAX_BUFFER_SIZE);
    if (!s_imgData) {
        printf("malloc failed\n");
        return -1;
    }

    s32Ret = pthread_create(&g_websocketThread, NULL, ThreadWebsocket, NULL);
    return s32Ret;
}

int app_ipc_WebSocket_DeInit()
{
    return 0;
}

int app_ipc_WebSocket_Stream_Send(void* pData, void* pArgs)
{
    if (pData == NULL) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "Web stream send pData is NULL\n");
        return -1;
    }

    if (g_wsi == NULL) {
        return 0;
    }

    VENC_STREAM_S* pstStream = (VENC_STREAM_S*)pData;
    VENC_PACK_S* ppack;
    unsigned char* pAddr = NULL;
    CVI_U32 packSize = 0;
    CVI_U32 total_packSize = 0;

    pthread_mutex_lock(&g_mutexLock);
    for (CVI_U32 i = 0; i < pstStream->u32PackCount; i++) {
        ppack = &pstStream->pstPack[i];
        packSize = ppack->u32Len - ppack->u32Offset;
        total_packSize += packSize;
    }

    CVI_U32 need_size = LWS_PRE + 1 + total_packSize + 1; // one for type, one for reserve
    if (need_size > MAX_BUFFER_SIZE) {
        printf("buffer size is not ok\n");
        pthread_mutex_unlock(&g_mutexLock);
        return -1;
    }

    memset(s_imgData, 0, total_packSize + LWS_PRE + 1 + 1);
    s_imgData[LWS_PRE] = 0 & 0xff; // type0: h264 buf

    CVI_U32 len = 0;
    for (CVI_U32 i = 0; i < pstStream->u32PackCount; i++) {
        ppack = &pstStream->pstPack[i];
        pAddr = ppack->pu8Addr + ppack->u32Offset;
        packSize = ppack->u32Len - ppack->u32Offset;
        memcpy(s_imgData + LWS_PRE + 1 + len, pAddr, packSize);
        len += packSize;
    }

    s_fileSize = len + 1;

    if (g_wsi != NULL) {
        lws_callback_on_writable(g_wsi);
    } else {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "WebSocketSend g_wsi is NULL\n");
    }

    pthread_mutex_unlock(&g_mutexLock);

    return 0;
}

EXTERN_C_END()
