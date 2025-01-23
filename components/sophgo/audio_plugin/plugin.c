#include <alsa/asoundlib.h>
#include <alsa/pcm_external.h>
#include <alsa/pcm_plugin.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cvi_aio.h"

typedef struct cvi_pcm_plugin {
    snd_pcm_ioplug_t io;
    cvi_ain_t ain;
    cvi_aout_t aout;

    snd_pcm_format_t format;
    unsigned int channels;
    unsigned int rate;
    snd_pcm_sframes_t frames_count;
} cvi_pcm_plugin_t;

static int init(cvi_pcm_plugin_t* plugin)
{
    plugin->format = SND_PCM_FORMAT_S16;
    plugin->channels = CHANNELS;
    plugin->rate = SAMPLE_RATE;

    return 0;
}

static int start(snd_pcm_ioplug_t* io)
{
    return 0;
}

static int stop(snd_pcm_ioplug_t* io)
{
    cvi_pcm_plugin_t* plugin = (cvi_pcm_plugin_t*)io;
    CVI_AIO_DBG("%s,%d: io->stream=%d\n", __FUNCTION__, __LINE__, io->stream);
    if (io->stream == SND_PCM_STREAM_CAPTURE) {
        if (CVI_SUCCESS != cvi_ain_deinit(&plugin->ain)) {
            return -EINVAL;
        }
    } else if (io->stream == SND_PCM_STREAM_PLAYBACK) {
        if (CVI_SUCCESS != cvi_aout_deinit(&plugin->aout)) {
            return -EINVAL;
        }
    }

    return 0;
}

static snd_pcm_sframes_t pointer(snd_pcm_ioplug_t* io)
{
    cvi_pcm_plugin_t* plugin = (cvi_pcm_plugin_t*)io;
    CVI_AIO_DBG("%s,%d: [%s]frames_count=%d\n", __FUNCTION__, __LINE__,
        (io->stream == SND_PCM_STREAM_PLAYBACK) ? "play" : "cap", plugin->frames_count);
    return plugin->frames_count;
}

static snd_pcm_sframes_t transfer_in(snd_pcm_ioplug_t* io, const snd_pcm_channel_area_t* areas, snd_pcm_uframes_t offset, snd_pcm_uframes_t frames)
{
    cvi_pcm_plugin_t* plugin = (cvi_pcm_plugin_t*)io;
    size_t bytes_per_frame = snd_pcm_format_width(plugin->format) / 8 * plugin->channels;
    size_t total_bytes = frames * bytes_per_frame;

    CVI_AIO_DBG("%s,%d: offset=%d, frames=%d, bytes_per_frame=%d, total_bytes=%d\n", __FUNCTION__, __LINE__,
        offset, frames, bytes_per_frame, total_bytes);

    size_t read_bytes = 0;
    unsigned char* addr = areas[0].addr;
    addr += areas[0].first / 8;
    addr += offset * bytes_per_frame;

    AUDIO_FRAME_S stFrame;
    if (CVI_SUCCESS == cvi_ain_get_frame(&plugin->ain, &stFrame)) {
        read_bytes = stFrame.u32Len * plugin->channels * bytes_per_frame;
        CVI_AIO_DBG("%s,%d: read_bytes=%d\n", __FUNCTION__, __LINE__, read_bytes);
        if (read_bytes > total_bytes) { // ToDo: need fifo
            read_bytes = total_bytes;
        }
        memcpy(addr, stFrame.u64VirAddr[0], read_bytes);
        frames = read_bytes / bytes_per_frame;
        CVI_AIO_DBG("%s,%d: frames=%d\n", __FUNCTION__, __LINE__, frames);
    }
    if (read_bytes < total_bytes) {
        memset(addr + read_bytes, 0, total_bytes - read_bytes);
        frames = total_bytes / bytes_per_frame;
        CVI_AIO_DBG("%s,%d: frames=%d\n", __FUNCTION__, __LINE__, frames);
    }
    plugin->frames_count += frames;

    return frames;
}

static snd_pcm_sframes_t transfer_out(snd_pcm_ioplug_t* io, const snd_pcm_channel_area_t* areas, snd_pcm_uframes_t offset, snd_pcm_uframes_t frames)
{
    cvi_pcm_plugin_t* plugin = (cvi_pcm_plugin_t*)io;
    size_t bytes_per_frame = snd_pcm_format_width(plugin->format) / 8 * plugin->channels;
    size_t total_bytes = frames * bytes_per_frame;

    if (NULL == plugin->aout.frame_buf) {
        return 0;
    }

    CVI_AIO_DBG("%s,%d: offset=%d, frames=%d, bytes_per_frame=%d, total_bytes=%d\n", __FUNCTION__, __LINE__,
        offset, frames, bytes_per_frame, total_bytes);

    unsigned char* addr = areas[0].addr;
    addr += areas[0].first / 8;
    addr += offset * bytes_per_frame;

    size_t write_bytes = plugin->aout.frame_size;
    uint32_t count = total_bytes / write_bytes;
    if (total_bytes % write_bytes) {
        count++;
    }
    CVI_AIO_DBG("%s,%d: count=%d\n", __FUNCTION__, __LINE__, count);

    frames = 0;
    for (uint32_t i = 0; i < count; i++) {
        CVI_AIO_DBG("%s,%d: [%d]write_bytes=%d\n", __FUNCTION__, __LINE__, i, write_bytes);
        memset(plugin->aout.frame_buf, 0, write_bytes);
        memcpy(plugin->aout.frame_buf, addr + (i * write_bytes), total_bytes - (i * write_bytes));
        if (cvi_aout_put_frame(&plugin->aout) != CVI_SUCCESS) {
            break;
        }
        frames += write_bytes / bytes_per_frame;
    }
    CVI_AIO_DBG("%s,%d: frames=%d\n", __FUNCTION__, __LINE__, frames);
    plugin->frames_count += frames;

    return frames;
}

static snd_pcm_sframes_t transfer(snd_pcm_ioplug_t* io, const snd_pcm_channel_area_t* areas, snd_pcm_uframes_t offset, snd_pcm_uframes_t frames)
{
    CVI_AIO_DBG("%s,%d: io->stream=%d\n", __FUNCTION__, __LINE__, io->stream);
    if (io->stream == SND_PCM_STREAM_CAPTURE) {
        return transfer_in(io, areas, offset, frames);
    } else if (io->stream == SND_PCM_STREAM_PLAYBACK) {
        return transfer_out(io, areas, offset, frames);
    }
}

static int close_cb(snd_pcm_ioplug_t* io)
{
    CVI_AIO_DBG("%s,%d\n", __FUNCTION__, __LINE__);
    cvi_pcm_plugin_t* plugin = (cvi_pcm_plugin_t*)io;
    free(plugin);

    return 0;
}

static int hw_params(snd_pcm_ioplug_t* io, snd_pcm_hw_params_t* params)
{
    CVI_AIO_DBG("%s,%d: io->period_size=%d\n", __FUNCTION__, __LINE__, io->period_size);
    CVI_AIO_DBG("%s,%d: io->buffer_size=%d\n", __FUNCTION__, __LINE__, io->buffer_size);

    cvi_pcm_plugin_t* plugin = (cvi_pcm_plugin_t*)io;
    CVI_AIO_DBG("%s,%d: io->stream=%d\n", __FUNCTION__, __LINE__, io->stream);
    if (io->stream == SND_PCM_STREAM_CAPTURE) {
        cvi_ain_params(&plugin->ain);
        plugin->ain.period_size = io->period_size;
        if (CVI_SUCCESS != cvi_ain_init(&plugin->ain)) {
            SNDERR("cvi_ain_init, err=%d", CVI_FAILURE);
            return -EINVAL;
        }
        plugin->frames_count = plugin->ain.stAudinAttr.u32PtNumPerFrm;
    } else if (io->stream == SND_PCM_STREAM_PLAYBACK) {
        cvi_aout_params(&plugin->aout);
        plugin->aout.period_size = io->period_size;
        if (CVI_SUCCESS != cvi_aout_init(&plugin->aout)) {
            SNDERR("cvi_aout_init, err=%d", CVI_FAILURE);
            return -EINVAL;
        }
        plugin->frames_count = 0;
    }

    return 0;
}

SND_PCM_PLUGIN_DEFINE_FUNC(cvi_audio)
{
    int err = 0;

    CVI_AIO_DBG("%s,%d: %s\n", __FUNCTION__, __LINE__, __TIME__);
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
        .start = start,
        .stop = stop,
        .pointer = pointer,
        .transfer = transfer,
        .close = close_cb,
        .hw_params = hw_params,
    };

    snd_pcm_ioplug_t* io = &plugin->io;

    io->version = SND_PCM_IOPLUG_VERSION;
    io->name = "cvi_audio";
    io->flags = SND_PCM_IOPLUG_FLAG_MONOTONIC;
    io->mmap_rw = 0;
    io->callback = &callback;
    io->private_data = plugin;

    CVI_AIO_DBG("%s,%d: name=%s, stream=%d, mode=%d\n", __FUNCTION__, __LINE__, name, stream, mode);
    err = snd_pcm_ioplug_create(&plugin->io, name, stream, mode);
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
