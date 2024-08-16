#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/prctl.h>
#include <unistd.h>
#include <stddef.h>
#include "app_ipcam_ll.h"


#define LL_DATA_CACHE_DEPTH_MAX     10

#define LL_INIT(N) ((N)->next = (N)->prev = (N))

#define LL_HEAD(H) struct llhead H = { &H, &H }

#define LL_ENTRY(P,T,N) ((T *)((char *)(P) - offsetof(T, N)))

#define LL_ADD(H, N) do {       \
    ((H)->next)->prev = (N);    \
    (N)->next = ((H)->next);    \
    (N)->prev = (H);            \
    (H)->next = (N);            \
} while (0)

#define LL_TAIL(H, N) do {      \
    ((H)->prev)->next = (N);    \
    (N)->prev = ((H)->prev);    \
    (N)->next = (H);            \
    (H)->prev = (N);            \
} while (0)

#define LL_DEL(N) do {                  \
    ((N)->next)->prev = ((N)->prev);    \
    ((N)->prev)->next = ((N)->next);    \
    LL_INIT(N);                         \
} while (0)

#define LL_EMPTY(N) ((N)->next == (N))

#define LL_FOREACH(H,N) for (N = (H)->next; N != (H); N = (N)->next)

#define LL_FOREACH_SAFE(H,N,T)  for (N = (H)->next, T = (N)->next; N != (H); N = (T), T = (N)->next)

#define LL_DATA_NODE_DEL(pNode) do {                \
    if (pNode != NULL) {                            \
        ((pNode)->next)->prev = ((pNode)->prev);    \
        ((pNode)->prev)->next = ((pNode)->next);    \
    }                                               \
} while(0)

#define container_of(ptr, type, member) ({          \
    const typeof(((type *)0)->member) *mptr = ptr;  \
    (type *)((char *)mptr-offsetof(type, member));  \
})

#define LINK_LIST_DATA_POP_FRONT(PopPoint, Head, member) ({             \
    if(Head->next == Head) {                                            \
        PopPoint = NULL;                                                \
    } else {                                                            \
        PopPoint = container_of(Head->next, typeof(*PopPoint), member); \
        LL_DATA_NODE_DEL(Head->next);                                   \
    }                                                                   \
})

#define LINK_LIST_DATA_PUSH_TAIL(H, N) do { \
    ((H)->prev)->next = (N);                \
    (N)->prev = ((H)->prev);                \
    (N)->next = (H);                        \
    (H)->prev = (N);                        \
} while (0)

int app_ipcam_LList_Data_Pop(void **pData, void *pArgs)
{
    APP_DATA_CTX_S *pstDataCtx = (APP_DATA_CTX_S *)pArgs;

    APP_LINK_LIST_S *pHeadLink = &pstDataCtx->stHead.link;
    if (LL_EMPTY(pHeadLink)) {
        return -1;
    }

    APP_DATA_LL_S *pNodePop = NULL;

    pthread_mutex_lock(&pstDataCtx->mutex);
    LINK_LIST_DATA_POP_FRONT(pNodePop, pHeadLink, link);
    if(pNodePop) {
        pstDataCtx->LListDepth--;
    }
    pthread_mutex_unlock(&pstDataCtx->mutex);

    if(pNodePop) {
        *pData = pNodePop->pData;
        free(pNodePop);
    } else {
        return -1;
    }

    return 0;
}

int app_ipcam_LList_Data_Push(void *pData, void *pArgs)
{
    if (pData == NULL || pArgs == NULL) {
        printf("pData or pArgs is NULL!\n");
        return -1;
    }

    APP_DATA_CTX_S *pstDataCtx = (APP_DATA_CTX_S *)pArgs;
    APP_DATA_PARAM_S *pstDataParam = &pstDataCtx->stDataParam;
    APP_DATA_LL_S *pHead = &pstDataCtx->stHead;

    if (!pstDataCtx->bRunStatus) {
        printf("Link List Cache Not Running Now!!\n");
        return -1;
    }

    APP_DATA_LL_S *pNewNode = NULL;
    pNewNode = (APP_DATA_LL_S *)malloc(sizeof(APP_DATA_LL_S));
    if(pNewNode == NULL) {
        printf("pNewNode malloc failed!\n");
        return -1;
    }

    pNewNode->pData = NULL;

    if (pstDataParam->fpDataSave(&pNewNode->pData, pData) != 0) {
        free(pNewNode);
        printf("data save failded!\n");
        return -1;
    }

    if (pstDataCtx->LListDepth > LL_DATA_CACHE_DEPTH_MAX) {
        void *pDataDrop = NULL;
        printf("LL cache is full and drop data. (LList depth:%d > Max:%d) \n", pstDataCtx->LListDepth, LL_DATA_CACHE_DEPTH_MAX);
        if (app_ipcam_LList_Data_Pop(&pDataDrop, pArgs) != 0) {
            free(pNewNode);
            printf("LL data drop failded!\n");
            return -1;
        }
        if(pDataDrop) {
            if(pstDataParam->fpDataFree) {
                pstDataParam->fpDataFree(&pDataDrop);
            }
            pDataDrop = NULL;
        }
    }

    pthread_mutex_lock(&pstDataCtx->mutex);
    APP_LINK_LIST_S *pHeadLink = &pHead->link;
    APP_LINK_LIST_S *pNodeLink = &pNewNode->link;
    if (pHeadLink == NULL || pNodeLink == NULL) {
        printf(" pHeadLink or pNodeLink is NULL\n");
        pthread_mutex_unlock(&pstDataCtx->mutex);
        return -1;
    }

    LINK_LIST_DATA_PUSH_TAIL(pHeadLink, pNodeLink);

    pstDataCtx->LListDepth++;
    pthread_mutex_unlock(&pstDataCtx->mutex);

    return 0;

}

static void *Thread_LList_Data_Consume(void *pArgs)
{
    APP_DATA_CTX_S *pstDataCtx = (APP_DATA_CTX_S *)pArgs;
    APP_DATA_PARAM_S *pstDataParam = &pstDataCtx->stDataParam;
    
    void *pData = NULL;
    char TaskName[32] = {0};

    sprintf(TaskName, "DataConsume");
    prctl(PR_SET_NAME, TaskName, 0, 0, 0);
    while(pstDataCtx->bRunStatus) {
        if(app_ipcam_LList_Data_Pop(&pData, pArgs) == 0) {
            if(pData != NULL) {
                if(pstDataParam->fpDataHandle) {
                    pstDataParam->fpDataHandle(pData, pArgs);
                }
                if(pstDataParam->fpDataFree) {
                    pstDataParam->fpDataFree(&pData);
                }
                pData = NULL;
            }
        } else {
            usleep(5*1000);
        }
    }

    return NULL;
}

int app_ipcam_LList_Data_Init(void * *pCtx, void *pParam)
{
    if ((pCtx == NULL) || (pParam == NULL)) {
        printf("pCtx or pParam is NULL\n");
        return -1;
    }

    int s32Ret = 0;
    APP_DATA_PARAM_S *pDataParam = (APP_DATA_PARAM_S *)pParam;
    APP_DATA_CTX_S *pDataCtx = (APP_DATA_CTX_S *)malloc(sizeof(APP_DATA_CTX_S));
    if (pDataCtx == NULL) {
        printf("pDataCtx is NULL\n");
        return -1;
    }

    pDataCtx->stDataParam.pParam = (void *)pDataParam->pParam;
    pDataCtx->stDataParam.fpDataSave   = pDataParam->fpDataSave;
    pDataCtx->stDataParam.fpDataFree   = pDataParam->fpDataFree;
    pDataCtx->stDataParam.fpDataHandle = pDataParam->fpDataHandle;
    for (int i = 0; i < APP_DATA_COMSUMES_MAX; i++) {
        pDataCtx->stDataParam.fpDataConsumes[i] = pDataParam->fpDataConsumes[i];
    }

    pDataCtx->LListDepth = 0;

    pthread_mutex_init(&pDataCtx->mutex, NULL);

    /* init link list head */
    LL_INIT(&pDataCtx->stHead.link);

    pDataCtx->bRunStatus = true;
    s32Ret = pthread_create(&pDataCtx->pthread_id,
                            NULL,
                            Thread_LList_Data_Consume,
                            (void *)pDataCtx);
    if (s32Ret != 0) {
        pthread_mutex_destroy(&pDataCtx->mutex);
        goto EXIT;
    }

    *pCtx = pDataCtx;

    return s32Ret;

EXIT:
    if(pDataCtx != NULL) {
        free(pDataCtx);
    }
    pDataCtx->bRunStatus = false;

    return s32Ret;
}

int app_ipcam_LList_Data_DeInit(void * *pCtx)
{
    if ((pCtx == NULL) || (*pCtx == NULL)) {
        printf("pCtx or *pCtx is NULL\n");
        return -1;
    }

    APP_DATA_CTX_S *pstDataCtx = *pCtx;
    APP_DATA_PARAM_S *pstDataParam = &pstDataCtx->stDataParam;

    void *pDataDrop = NULL;
    pstDataCtx->bRunStatus = false;

    pthread_join(pstDataCtx->pthread_id, NULL);

    while(app_ipcam_LList_Data_Pop(&pDataDrop, *pCtx) == 0) {
        if(pDataDrop) {
            if(pstDataParam->fpDataFree) {
                pstDataParam->fpDataFree(&pDataDrop);
            }
            pDataDrop = NULL;
        }
    }
    pstDataCtx->LListDepth = 0;
    pthread_mutex_destroy(&pstDataCtx->mutex);

    free(*pCtx);
    *pCtx = NULL;

    return 0;
}
