#ifndef __APP_IPCAM_PARAM_PARSE_H__
#define __APP_IPCAM_PARAM_PARSE_H__

#include "linux/cvi_common.h"
#include "linux/cvi_comm_video.h"
#include "app_ipcam_comm.h"
#include "app_ipcam_sys.h"
#include "app_ipcam_vi.h"
#include "app_ipcam_vpss.h"
#ifdef OSD_SUPPORT
#include "app_ipcam_osd.h"
#endif
#include "app_ipcam_venc.h"
#include "app_ipcam_rtsp.h"
#include "app_ipcam_dump.h"
#include "app_ipcam_mq.h"
#ifdef AUDIO_SUPPORT
#include "app_ipcam_audio.h"
#endif

#ifdef AI_SUPPORT
#include "app_ipcam_ai.h"
#endif
#ifdef RECORD_SUPPORT
#include "app_ipcam_record.h"
#endif

#ifdef __cplusplus
extern "C"
{
#endif

#define  CODEC_TYPE_H264    "264"
#define  CODEC_TYPE_H265    "265"
#define  CODEC_TYPE_MJP     "mjp"
#define  CODEC_TYPE_JPG     "jpg"

#define MAIN_STREAM_SIZE_W   2560
#define MAIN_STREAM_SIZE_H   1440

#define SUB_STREAM_SIZE_W    1280
#define SUB_STREAM_SIZE_H    720

#define CAP_JPG_SIZE_W      1280
#define CAP_JPG_SIZE_H      720

#define VI_FRAMERATE			15
#define VPSS_FRAMERATE			VI_FRAMERATE
#define VENC_INPUT_FRAMERATE	VPSS_FRAMERATE
#define VENC0_OUTPUT_FRAMERATE	15
#define VENC1_OUTPUT_FRAMERATE	15

typedef enum APP_STREAM_TO_T {
    TO_RTSP     = 0x0001,
    TO_FLASH    = 0x0010

} APP_STREAM_TO_E;

int app_ipcam_Param_Load(void);

int app_ipcam_Opts_Parse(int argc, char *argv[]);

#ifdef __cplusplus
}
#endif

#endif
