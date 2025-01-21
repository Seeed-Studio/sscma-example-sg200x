#ifndef __CVI_AIN_H__
#define __CVI_AIN_H__

#include "audio.h"

#define SAMPLE_RATE 16000
#define CHANNELS 1

typedef struct cvi_ain {
    int AiDev;
    int AiChn;
    AIO_ATTR_S stAudinAttr;
    AI_TALKVQE_CONFIG_S stAiVqeTalkAttr;
} cvi_ain_t;

CVI_S32 cvi_audio_init(cvi_ain_t* ain);
CVI_S32 cvi_audio_get_frame(cvi_ain_t* ain, AUDIO_FRAME_S* pstFrame, AEC_FRAME_S* pstAecFrm);
CVI_S32 cvi_audio_deinit(cvi_ain_t* ain);

#endif // __CVI_AIN_H__
