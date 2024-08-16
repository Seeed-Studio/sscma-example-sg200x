#ifndef __APP_IPCAM_LL_H__
#define __APP_IPCAM_LL_H__
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define APP_LL_CONTEXT_MAX      8
#define APP_DATA_COMSUMES_MAX   8

typedef int (*pfpDataSave)(void **dst, void *src);
typedef int (*pfpDataFree)(void **src);
typedef void (*pfpDataHandle)(void *data, void *param);
typedef int (*pfpDataConsumes)(void *pData, void *pCtx);

typedef struct llhead {
    struct llhead *prev;
    struct llhead *next;
} APP_LINK_LIST_S;

typedef struct APP_DATA_LL_T {
    APP_LINK_LIST_S link;
    void *pData;
} APP_DATA_LL_S;

typedef struct APP_DATA_PARAM_T {
    void *pParam;
    pfpDataSave fpDataSave;
    pfpDataFree fpDataFree;
    pfpDataHandle fpDataHandle;
    pfpDataConsumes fpDataConsumes[APP_DATA_COMSUMES_MAX];
} APP_DATA_PARAM_S;

typedef struct APP_DATA_CTX_T {
    APP_DATA_LL_S stHead;
    int LListDepth;
    bool bRunStatus;
    pthread_t pthread_id;
    pthread_mutex_t mutex;
    APP_DATA_PARAM_S stDataParam;
} APP_DATA_CTX_S;

int app_ipcam_LList_Data_Init(void * *pCtx, void *pParam);
int app_ipcam_LList_Data_DeInit(void * *pCtx);
int app_ipcam_LList_Data_Push(void *pData, void *pArgs);

#ifdef __cplusplus
}
#endif

#endif






