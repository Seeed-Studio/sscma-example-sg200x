#ifndef __SOPHGO_AUDIO_H__
#define __SOPHGO_AUDIO_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <linux/cvi_comm_sys.h>
#include <linux/cvi_common.h>
#include <linux/cvi_type.h>

#include "acodec.h"
#include "cvi_audio.h"
#include "cvi_audio_aac_adp.h"
#include "cvi_comm_aio.h"

typedef int (*audio_frame_handler)(AUDIO_FRAME_S* pFrame);
typedef int (*audio_stream_handler)(AUDIO_STREAM_S* pStream);

// rate=[AUDIO_SAMPLE_RATE_8K, AUDIO_SAMPLE_RATE_16K] (VQE only support on 8k/16k sample rate)
// eType=[PT_G726, PT_G711A, PT_G711U, PT_ADPCMA, PT_AAC]
int startAudioIn(AUDIO_SAMPLE_RATE_E rate, PAYLOAD_TYPE_E enType,
    audio_frame_handler frame_out, audio_stream_handler stream_out);
int stopAudioIn(void);
int setAudioInVolume(uint8_t volumedb);

int startAudioOut(AUDIO_SAMPLE_RATE_E rate, PAYLOAD_TYPE_E enType,
    audio_stream_handler stream_in, audio_frame_handler frame_out);
int stopAudioOut(void);

#ifdef __cplusplus
}
#endif

#endif // __SOPHGO_AUDIO_H__