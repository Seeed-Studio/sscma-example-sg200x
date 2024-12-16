#ifndef __SOPHGO_AIO_H__
#define __SOPHGO_AIO_H__

#include "audio.h"
#include "app_ipcam_comm.h"

typedef struct thread_aio_context {
    uint8_t is_running;
    pthread_t thread;
    audio_frame_handler frame_handler;
    audio_stream_handler stream_handler;

    AUDIO_SAMPLE_RATE_E enRate;
    PAYLOAD_TYPE_E enType;
} thread_aio_context_t;

extern void* Thread_AudioIn_Proc(void* arg);
// extern void* Thread_AudioOut_Proc(void* arg);

#endif // __SOPHGO_AIO_H__