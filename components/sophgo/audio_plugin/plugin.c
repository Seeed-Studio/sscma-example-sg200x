#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <alsa/asoundlib.h>
#include <alsa/pcm_external.h>
#include <alsa/pcm_plugin.h>

SND_PCM_PLUGIN_DEFINE_FUNC(cvi_audio) {
}

SND_PCM_PLUGIN_SYMBOL(cvi_audio);
