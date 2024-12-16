#include <iostream>
#include <signal.h>
#include <syslog.h>
#include <unistd.h>

#include "audio.h"

static CVI_VOID app_ipcam_ExitSig_handle(CVI_S32 signo)
{
    signal(SIGINT, SIG_IGN);
    signal(SIGTERM, SIG_IGN);

    if ((SIGINT == signo) || (SIGTERM == signo)) {
        printf("AAAAAAAAAAAAAAAAA:app_ipcam_ExitSig_handle\n");
        stopAudioIn();
    }

    exit(-1);
}

static void dump_audiodata(char* filename, char* buf, unsigned int len)
{
    if (filename == NULL) {
        return;
    }

    FILE* fp = fopen(filename, "ab+");
    fwrite(buf, 1, len, fp);
    fclose(fp);
}

int ai_handler(AUDIO_FRAME_S* pFrame)
{
    // printf("audio_frame_handler\n");
    if (pFrame) {
        // struct timespec end;
        // struct timespec now;
        // clock_gettime(CLOCK_MONOTONIC, &now);

        int channels = (pFrame->enSoundmode == AUDIO_SOUND_MODE_MONO) ? 1 : 2;
        dump_audiodata((char*)"AiData_16K.raw", (char*)pFrame->u64VirAddr[0],
            pFrame->u32Len * channels * 2);

        // clock_gettime(CLOCK_MONOTONIC, &end);
        // printf("audio_frame_handler cost %ld ns\n",
        //     (end.tv_sec - now.tv_sec) * 1000000000 + end.tv_nsec - now.tv_nsec);
    }

    return 0;
}

int aenc_handler(AUDIO_STREAM_S* pStream)
{
    // printf("audio_stream_handler\n");
    return 0;
}

int main(int argc, char* argv[])
{
    signal(SIGINT, app_ipcam_ExitSig_handle);
    signal(SIGHUP, app_ipcam_ExitSig_handle);
    signal(SIGTERM, app_ipcam_ExitSig_handle);

    setAudioInVolume(0);
    startAudioIn((AUDIO_SAMPLE_RATE_E)16000, PT_AAC, ai_handler, NULL);
    setAudioInVolume(16);

    while (1) {
        sleep(1);
    }

    return 0;
}