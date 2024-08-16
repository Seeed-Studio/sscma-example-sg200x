#ifndef __APP_DBG_H__
#define __APP_DBG_H__

#include <unistd.h>
#include <inttypes.h>
#include <pthread.h>

/* Make this header file easier to include in C++ code */
#ifdef __cplusplus
extern "C" {
#endif

#define APP_IPCAM_MAX_FILE_LEN 128
#define APP_IPCAM_MAX_STR_LEN 64

#define APP_IPCAM_SUCCESS           ((int)(0))
#define APP_IPCAM_ERR_FAILURE       ((int)(-1001))
#define APP_IPCAM_ERR_NOMEM         ((int)(-1002))
#define APP_IPCAM_ERR_TIMEOUT       ((int)(-1003))
#define APP_IPCAM_ERR_INVALID       ((int)(-1004))

#define IS_FILE_EXIST(FILE_PATH)                    \
    do {                                            \
        if (0 == access(FILE_PATH, F_OK)) {         \
            printf("%s exist!\n", FILE_PATH);       \
        } else {                                    \
            printf("%s not exist!\n", FILE_PATH);   \
        }                                           \
    } while(0)

#define APP_CHK_RET(express, name)                                                                              \
    do {                                                                                                        \
        int rc = express;                                                                                       \
        if (rc != CVI_SUCCESS) {                                                                                \
            printf("\033[40;31m%s failed at %s line:%d with %#x!\033[0m\n", name, __FUNCTION__, __LINE__, rc);  \
            return rc;                                                                                          \
        }                                                                                                       \
    } while (0)

#define APP_IPCAM_LOGE(...)   printf(__VA_ARGS__)
#define APP_IPCAM_CHECK_RET(ret, fmt...)                                \
    do {                                                                \
        if (ret != CVI_SUCCESS) {                                       \
            APP_IPCAM_LOGE(fmt);                                        \
            APP_IPCAM_LOGE("fail and return:[%#x]\n", ret);             \
            return ret;                                                 \
        }                                                               \
    } while (0)

#define CHECK_CONTI(express, point, _string)            \
    do {                                                \
        int rc = express;                               \
        if (rc != 0) {                                  \
            printf("\n%s with rc:0x%x!", _string, rc);  \
            pthread_mutex_unlock(point);                \
            continue;                                   \
        }                                               \
    } while(0)

#ifndef _NULL_POINTER_CHECK_
#define _NULL_POINTER_CHECK_(p, errcode)    \
    do {                                        \
        if (!(p)) {                             \
            printf("pointer[%s] is NULL\n", #p); \
            return errcode;                     \
        }                                       \
    } while (0)
#endif

#define APP_IPCAM_CODEC_CHECK(STR, CODEC) do {          \
        if (!strcmp(STR, "265")) CODEC = PT_H265;       \
        else if (!strcmp(STR, "264")) CODEC = PT_H264;  \
        else if (!strcmp(STR, "jpg")) CODEC = PT_JPEG;  \
        else if (!strcmp(STR, "mjp")) CODEC = PT_MJPEG; \
        else {                                          \
            printf("STR=%s is illegal\n", STR);         \
            return -1;                                  \
        }                                               \
    } while(0)

#define APP_IPCAM_RCMODE_CHECK(STR, RCMODE) do {                                \
        if (!strcmp(STR, "h264cbr"))    RCMODE = VENC_RC_MODE_H264CBR;          \
        else if (!strcmp(STR, "264vbr"))    RCMODE = VENC_RC_MODE_H264VBR;      \
        else if (!strcmp(STR, "264avbr"))   RCMODE = VENC_RC_MODE_H264AVBR;     \
        else if (!strcmp(STR, "264qvbr"))   RCMODE = VENC_RC_MODE_H264QVBR;     \
        else if (!strcmp(STR, "264fixqp"))  RCMODE = VENC_RC_MODE_H264FIXQP;    \
        else if (!strcmp(STR, "264qpmap"))  RCMODE = VENC_RC_MODE_H264QPMAP;    \
        else if (!strcmp(STR, "mjpgcbr"))    RCMODE = VENC_RC_MODE_MJPEGCBR;    \
        else if (!strcmp(STR, "mjpgvbr"))    RCMODE = VENC_RC_MODE_MJPEGVBR;    \
        else if (!strcmp(STR, "mjpgfixqp"))  RCMODE = VENC_RC_MODE_MJPEGFIXQP;  \
        else if (!strcmp(STR, "265cbr"))    RCMODE = VENC_RC_MODE_H265CBR;      \
        else if (!strcmp(STR, "265vbr"))    RCMODE = VENC_RC_MODE_H265VBR;      \
        else if (!strcmp(STR, "265avbr"))   RCMODE = VENC_RC_MODE_H265AVBR;     \
        else if (!strcmp(STR, "265qvbr"))   RCMODE = VENC_RC_MODE_H265QVBR;     \
        else if (!strcmp(STR, "265fixqp"))  RCMODE = VENC_RC_MODE_H265FIXQP;    \
        else if (!strcmp(STR, "265qpmap"))  RCMODE = VENC_RC_MODE_H265QPMAP;    \
        else {                                                                  \
            printf("STR=%s is illegal\n", STR);                                 \
            return -1;                                                          \
        }                                                                       \
    } while(0)

/*
 * for ipcam cmd test 
 */

typedef enum APP_IPCAM_CMD_ID_E {
    CVI_CMD_VIDEO_ATTR      = 0,
    CVI_CMD_RECT_SWITCH     = 1,
    CVI_CMD_AUDIO_SWITCH    = 2,
    CVI_CMD_AI_PD_SWITCH    = 3,
    CVI_CMD_AI_MD_SWITCH    = 4,
    CVI_CMD_AI_FD_SWITCH    = 5,
    CVI_CMD_AUTO_RGB_IR_SWITCH = 6,
    CVI_CMD_PQ_CAT          = 7,
    CVI_CMD_FLIP_SWITCH     = 8,
    CVI_CMD_MIRROR_SWITCH   = 9,
    CVI_CMD_180_SWITCH      = 10,
    CVI_CMD_ROTATE_SWITCH   = 11,
    CVI_CMD_RECORD_SET      = 12,
    CVI_CMD_MP3_PLAY        = 13,
    CVI_CMD_BUT
} APP_IPCAM_CMD_ID_T;


/*  debug solution  */
/**
 * @brief  :  easy debug for app-development
 * @author :  xulong
 * @since  :  2021-06-17
 * @how to used   :	1. RPrintf("Hello world");
                    2. __FUNC_TRACK__;
                    3. APP_PROF_LOG_PRINT(LEVEL_TRACE, "Hello world argv[0] %s\n", argv[0]);
                    n. ......
* @note1 :  default debug level is LEVEL_INFO
*/
/* level setting */
#define LEVEL_ERROR                 0 /*min print info*/
#define LEVEL_WARN                  1
#define LEVEL_INFO                  2
#define LEVEL_TRACE                 3
#define LEVEL_DEBUG                 4 /*max print info*/

/* color setting */
#define DEBUG_COLOR_NORMAL          "\033[m"
#define DEBUG_COLOR_BLACK           "\033[30m"
#define DEBUG_COLOR_RED             "\033[31m"
#define DEBUG_COLOR_GREEN           "\033[32m"
#define DEBUG_COLOR_YELLOW          "\033[33m"
#define DEBUG_COLOR_BLUE            "\033[34m"
#define DEBUG_COLOR_PURPLE          "\033[35m"
#define DEBUG_COLOR_BKRED           "\033[41;37m"

/* tag setting */
#define STRINGIFY(x, fmt)           #x fmt
#define ADDTRACECTAG(fmt)           STRINGIFY([app][trace][%s %u], fmt)
#define ADDDBGTAG(fmt)              STRINGIFY([app][dbg][%s %u], fmt)
#define ADDTAG(fmt)                 STRINGIFY([app], fmt)
#define ADDWARNTAG(fmt)             STRINGIFY([app][warn][%s %u], fmt)
#define ADDERRTAG(fmt)              STRINGIFY([app][err][%s %u], fmt)

#ifndef DEF_DEBUG_LEVEL
#define DEF_DEBUG_LEVEL            	LEVEL_ERROR  //use level
#endif

#define APP_PROF_LOG_PRINT(level, fmt, args...)                                                                     \
        if(level <= DEF_DEBUG_LEVEL) {                                                                              \
            if(level <= LEVEL_WARN) {                                                                               \
                if(level == LEVEL_WARN)                                                                             \
                    printf(DEBUG_COLOR_YELLOW ADDWARNTAG(fmt) DEBUG_COLOR_NORMAL , __FUNCTION__, __LINE__, ##args); \
                else                                                                                                \
                    printf(DEBUG_COLOR_RED ADDERRTAG(fmt) DEBUG_COLOR_NORMAL , __FUNCTION__, __LINE__, ##args);     \
            }                                                                                                       \
            else if(level <= LEVEL_TRACE) {                                                                         \
                if(level == LEVEL_TRACE)                                                                            \
                    printf(DEBUG_COLOR_BLUE ADDWARNTAG(fmt) DEBUG_COLOR_NORMAL , __FUNCTION__, __LINE__, ##args);   \
                else                                                                                                \
                    printf( ADDTAG(fmt) , ##args);                                                                  \
            } else                                                                                                  \
                printf(DEBUG_COLOR_PURPLE ADDDBGTAG(fmt) DEBUG_COLOR_NORMAL , __FUNCTION__, __LINE__, ##args);      \
        }


#define RPrintf(str)    { printf(DEBUG_COLOR_RED    ADDTAG(str)  DEBUG_COLOR_NORMAL "\n"); }
#define GPrintf(str)    { printf(DEBUG_COLOR_GREEN  ADDTAG(str)  DEBUG_COLOR_NORMAL "\n"); }
#define BPrintf(str)    { printf(DEBUG_COLOR_BLUE   ADDTAG(str)  DEBUG_COLOR_NORMAL "\n"); }
#define YPrintf(str)    { printf(DEBUG_COLOR_YELLOW ADDTAG(str)  DEBUG_COLOR_NORMAL "\n"); }

#define __FUNC_TRACK__  APP_PROF_LOG_PRINT(LEVEL_INFO, "%s :%u\n", __FUNCTION__, __LINE__)

/* 	END debug API 
*	easy debug for app-development
*/

typedef void *(*pfp_task_entry)(void *param);

typedef struct _RUN_THREAD_PARAM
{
    int bRun_flag;
    pthread_t mRun_PID;
} RUN_THREAD_PARAM;

unsigned int GetCurTimeInMsec(void);

#ifdef __cplusplus
}
#endif


#endif
