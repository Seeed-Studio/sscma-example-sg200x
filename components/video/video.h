#ifndef _VIDEO_H_
#define _VIDEO_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

#include "app_ipcam_paramparse.h"

/*
vpss - grp0 - chn0(max 1920) - vencchn0 FILE(H264/H265/JPGE)
            - chn1(max 2880) - vencchn1 RTSP(H264/H265/JPGE)
            - chn2(max 1920) - vencchn2 AI(RGB888)
*/

typedef enum {
    VIDEO_FORMAT_RGB888 = 0,
    VIDEO_FORMAT_H264,
    VIDEO_FORMAT_H265,
    VIDEO_FORMAT_JPEG,

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

typedef struct {
    uint32_t width;
    uint32_t height;
    uint8_t* pdata;
    uint32_t size;
    uint32_t timestamp;
    uint32_t id;
} video_frame_t;


int initVideo(void);
int deinitVideo(void);
int startVideo(void);
int setupVideo(video_ch_index_t ch, const video_ch_param_t* param);
int registerVideoFrameHandler(video_ch_index_t ch, int index, pfpDataConsumes handler, void* pUserData);

#ifdef __cplusplus
}
#endif

#endif // _VIDEO_H_