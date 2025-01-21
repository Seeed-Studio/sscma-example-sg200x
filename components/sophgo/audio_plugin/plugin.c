#include <alsa/asoundlib.h>
#include <alsa/pcm_external.h>
#include <alsa/pcm_plugin.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cvi_ain.h"

typedef struct cvi_pcm_plugin {
    snd_pcm_ioplug_t io;
    cvi_ain_t ain;
    snd_pcm_format_t format;
    unsigned int channels;
    unsigned int rate;
    snd_pcm_sframes_t frames_count;
} cvi_pcm_plugin_t;

static int init(cvi_pcm_plugin_t* plugin) {
    plugin->format       = SND_PCM_FORMAT_S16;
    plugin->channels     = CHANNELS;
    plugin->rate         = SAMPLE_RATE;
    plugin->frames_count = SAMPLE_RATE;

    return 0;
}

static int start(snd_pcm_ioplug_t* io) {
    cvi_pcm_plugin_t* plugin = (cvi_pcm_plugin_t*)io;
    cvi_audio_init(&plugin->ain);
    return 0;
}

static int stop(snd_pcm_ioplug_t* io) {
    cvi_pcm_plugin_t* plugin = (cvi_pcm_plugin_t*)io;
    cvi_audio_deinit(&plugin->ain);
    return 0;
}

static snd_pcm_sframes_t pointer(snd_pcm_ioplug_t* io) {
    cvi_pcm_plugin_t* plugin = (cvi_pcm_plugin_t*)io;

    return plugin->frames_count;
}

static snd_pcm_sframes_t transfer(snd_pcm_ioplug_t* io, const snd_pcm_channel_area_t* areas, snd_pcm_uframes_t offset, snd_pcm_uframes_t frames) {
    cvi_pcm_plugin_t* plugin = (cvi_pcm_plugin_t*)io;
    size_t bytes_per_frame   = snd_pcm_format_width(plugin->format) / 8 * plugin->channels;
    size_t total_bytes       = frames * bytes_per_frame;

    // printf("%s,%d: offset=%d, frames=%d, bpf=%d, total_bytes=%d\n", __FUNCTION__, __LINE__,
    //     offset, frames, bytes_per_frame, total_bytes);

    size_t read_bytes   = 0;
    unsigned char* addr = areas[0].addr;
    addr += areas[0].first / 8;
    addr += offset * bytes_per_frame;

    AUDIO_FRAME_S stFrame;
    AEC_FRAME_S stAecFrm;
    if (CVI_SUCCESS == cvi_audio_get_frame(&plugin->ain, &stFrame, &stAecFrm)) {
        read_bytes = stFrame.u32Len * plugin->channels * bytes_per_frame;
        // printf("%s,%d: read_bytes=%d\n", __FUNCTION__, __LINE__, read_bytes);
        if (read_bytes > total_bytes) {
            read_bytes = total_bytes;
        }
        memcpy(addr, stFrame.u64VirAddr[0], read_bytes);
        plugin->frames_count += frames;
        frames = read_bytes / bytes_per_frame;
        // printf("%s,%d: frames=%d\n", __FUNCTION__, __LINE__, frames);
    }
    if (read_bytes < total_bytes) {
        memset(addr + read_bytes, 0, total_bytes - read_bytes);
        frames = read_bytes / bytes_per_frame;
        // printf("%s,%d: frames=%d\n", __FUNCTION__, __LINE__, frames);
    }

    return frames;
}

static int close_cb(snd_pcm_ioplug_t* io) {
    // printf("%s,%d\n", __FUNCTION__, __LINE__);
    cvi_pcm_plugin_t* plugin = (cvi_pcm_plugin_t*)io;
    free(plugin);

    return 0;
}

SND_PCM_PLUGIN_DEFINE_FUNC(cvi_audio) {
    int err = 0;

    // printf("%s,%d: %s\n", __FUNCTION__, __LINE__, __TIME__);

    cvi_pcm_plugin_t* plugin = calloc(1, sizeof(*plugin));
    if (!plugin) {
        SNDERR("Failed to allocate memory for plugin");
        return -ENOMEM;
    }

    if (init(plugin)) {
        err = -EINVAL;
        goto exit;
    }

    static const snd_pcm_ioplug_callback_t callback = {
        .start    = start,
        .stop     = stop,
        .pointer  = pointer,
        .transfer = transfer,
        .close    = close_cb,
    };

    snd_pcm_ioplug_t* io = &plugin->io;

    io->version      = SND_PCM_IOPLUG_VERSION;
    io->name         = "cvi_audio";
    io->flags        = SND_PCM_IOPLUG_FLAG_MONOTONIC;
    io->mmap_rw      = 0;
    io->callback     = &callback;
    io->private_data = plugin;

    err = snd_pcm_ioplug_create(&plugin->io, name, SND_PCM_STREAM_CAPTURE, mode);
    // printf("%s,%d: name=%s, mode=%d\n", __FUNCTION__, __LINE__, name, mode);
    if (err < 0) {
        SNDERR("snd_pcm_ioplug_create, err=%d", err);
        goto exit;
    }

    *pcmp = plugin->io.pcm;

    return 0;

exit:
    if (plugin) {
        free(plugin);
    }

    return err;
}

SND_PCM_PLUGIN_SYMBOL(cvi_audio);
