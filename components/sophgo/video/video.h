#ifndef _SOPHGO_VIDEO_H_
#define _SOPHGO_VIDEO_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "app_ipcam_paramparse.h"

typedef enum {
    VIDEO_FORMAT_RGB888 = 0, // no need venc
    VIDEO_FORMAT_NV21, // no need venc
    VIDEO_FORMAT_JPEG,
    VIDEO_FORMAT_H264,
    VIDEO_FORMAT_H265,

    VIDEO_FORMAT_COUNT
} video_format_t;

typedef enum {
    VIDEO_CH0 = 0,
    VIDEO_CH1,
    VIDEO_CH2,

    VIDEO_CH_MAX
} video_ch_index_t;

typedef struct {
    video_format_t format;
    uint32_t width;
    uint32_t height;
    uint8_t fps;
} video_ch_param_t;

// typedef struct {
//     uint32_t width;
//     uint32_t height;
//     uint8_t* pdata;
//     uint32_t size;
//     uint32_t timestamp;
//     uint32_t id;
// } video_frame_t;

int initVideo(void);
int deinitVideo(void);
int startVideo(void);
int setupVideo(video_ch_index_t ch, const video_ch_param_t* param);
int registerVideoFrameHandler(video_ch_index_t ch, int index, pfpDataConsumes handler, void* pUserData);

#ifdef __cplusplus
}
#endif

#endif // _SOPHGO_VIDEO_H_