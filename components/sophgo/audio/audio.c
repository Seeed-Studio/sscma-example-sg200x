#include "audio_io.h"

static thread_aio_context_t audio_in_context;

int startAudioIn(uint32_t rate, PAYLOAD_TYPE_E enType,
    audio_frame_handler frame_out, audio_stream_handler stream_out)
{
    CVI_S32 s32Ret = CVI_SUCCESS;

    if (audio_in_context.is_running) {
        return CVI_SUCCESS;
    }
    audio_in_context.enRate = (AUDIO_SAMPLE_RATE_E)rate;
    audio_in_context.enType = enType;
    audio_in_context.frame_handler = frame_out;
    audio_in_context.stream_handler = stream_out;

    pthread_attr_t pthread_attr;
    pthread_attr_init(&pthread_attr);
    struct sched_param param;
    param.sched_priority = 80;
    pthread_attr_setschedpolicy(&pthread_attr, SCHED_RR);
    pthread_attr_setschedparam(&pthread_attr, &param);
    pthread_attr_setinheritsched(&pthread_attr, PTHREAD_EXPLICIT_SCHED);

    s32Ret = pthread_create(&audio_in_context.thread, &pthread_attr, Thread_AudioIn_Proc, &audio_in_context);
    if (s32Ret != 0) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "pthread_create failed, ret = %d\n", s32Ret);
        return s32Ret;
    }

    // Thread_AudioIn_Proc(&audio_in_context);

    return CVI_SUCCESS;
}

int stopAudioIn(void)
{
    if (audio_in_context.is_running) {
        audio_in_context.is_running = 0;
    }

    if (audio_in_context.thread) {
        pthread_join(audio_in_context.thread, NULL);
        audio_in_context.thread = 0;
    }

    return CVI_SUCCESS;
}

/**
 * @brief Sets the audio input volume.
 *
 * This function sets the audio input volume to the specified level.
 * The volume level ranges from 0 (muted) to 24 decibels (dB).
 * If the specified volume exceeds 24 dB, it will be capped at 24 dB.
 *
 * @param volumedb The desired volume level in decibels (dB).
 *                 Valid range is [0, 24], where 0 means muted.
 *
 * @return Returns CVI_SUCCESS if the volume is set successfully,
 *         otherwise returns an error code indicating the failure.
 */
int setAudioInVolume(uint8_t volumedb)
{
    CVI_S32 s32Ret = CVI_SUCCESS;
    CVI_S32 idevid = 0;

    if (volumedb > 24) {
        volumedb = 24;
    }
    s32Ret = CVI_AI_SetVolume(idevid, (CVI_S32)volumedb);

    return s32Ret;
}

int startAudioOut(AUDIO_SAMPLE_RATE_E rate, PAYLOAD_TYPE_E enType,
    audio_stream_handler stream_in, audio_frame_handler frame_out)
{
}

int stopAudioOut(void)
{
}

int setAudioOutVolume(uint8_t volumedb)
{
    CVI_S32 s32Ret = CVI_SUCCESS;
    CVI_S32 idevid = 0;

    if (volumedb > 30) {
        volumedb = 30;
    }
    s32Ret = CVI_AI_SetVolume(idevid, (CVI_S32)volumedb);

    return s32Ret;
}