#include <iostream>
#include <signal.h>
#include <syslog.h>
#include <unistd.h>

#include "audio.h"

static CVI_VOID app_ipcam_ExitSig_handle(CVI_S32 signo) {
    signal(SIGINT, SIG_IGN);
    signal(SIGTERM, SIG_IGN);

    if ((SIGINT == signo) || (SIGTERM == signo)) {
        // deinitAudio();
    }

    exit(-1);
}

// static int fpSaveAccFrame(void* pData, void* pArgs, void* pUserData) {
//     return 0;
// }

int main(int argc, char* argv[]) {
    signal(SIGINT, app_ipcam_ExitSig_handle);
    signal(SIGHUP, app_ipcam_ExitSig_handle);
    signal(SIGTERM, app_ipcam_ExitSig_handle);

    // if (initAudio())
    //     return -1;

    startAudioIn((AUDIO_SAMPLE_RATE_E)16000, PT_AAC, NULL, NULL);

    // audio_ch_param_t param;

    // // ch0
    // setupAudio(AUDIO_CH0, &param);
    // registerAudioFrameHandler(AUDIO_CH0, 0, (void*)fpSaveAccFrame, NULL);

    // startAudio();

    while (1) {
        sleep(1);
    }

    return 0;
}