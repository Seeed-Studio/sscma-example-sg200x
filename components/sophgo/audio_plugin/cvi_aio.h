#ifndef __CVI_AIN_H__
#define __CVI_AIN_H__

#include "audio.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define CHANNELS 1
#define SAMPLE_RATE 16000

typedef struct cvi_ain {
    int ch_cnt;
    int sample_rate;
    int bytes_per_sample;
    int period;
    bool vqe_on;

    CVI_S32 AiDev;
    CVI_S32 AiChn;
    AIO_ATTR_S stAudinAttr;
    AI_TALKVQE_CONFIG_S stAiVqeTalkAttr;
} cvi_ain_t;

typedef struct cvi_aout {
    int ch_cnt;
    int sample_rate;
    int bytes_per_sample;
    int period;
    bool vqe_on;
    void* frame_buf;
    uint32_t frame_size;

    CVI_S32 AoChn;
    CVI_S32 AoDev;
    AIO_ATTR_S stAudoutAttr;
    AO_VQE_CONFIG_S stVqeConfig;
} cvi_aout_t;

CVI_S32 cvi_ain_init(cvi_ain_t* ain);
CVI_S32 cvi_ain_get_frame(cvi_ain_t* ain, AUDIO_FRAME_S* pstFrame);
CVI_S32 cvi_ain_deinit(cvi_ain_t* ain);

CVI_S32 cvi_aout_init(cvi_aout_t* aout);
CVI_S32 cvi_aout_put_frame(cvi_aout_t* aout);
CVI_S32 cvi_aout_deinit(cvi_aout_t* aout);

#ifdef __cplusplus
}
#endif

#endif // __CVI_AIN_H__
