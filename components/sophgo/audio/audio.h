#ifndef _AUDIO_H_
#define _AUDIO_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "app_ipcam_paramparse.h"

typedef enum {
    AUDIO_CH0 = 0,
    AUDIO_CH1,

    AUDIO_CH_MAX
} audio_ch_index_t;

typedef struct {
    uint8_t fps;
} audio_ch_param_t;

int initAudio(void);
int deinitAudio(void);
int startAudio(void);
int setupAudio(audio_ch_index_t ch, const audio_ch_param_t* param);
int registerAudioFrameHandler(audio_ch_index_t ch, int index, void* handler, void* pUserData);

#ifdef __cplusplus
}
#endif

#endif // _AUDIO_H_