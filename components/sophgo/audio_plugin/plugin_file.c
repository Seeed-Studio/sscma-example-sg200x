#include <alsa/asoundlib.h>
#include <alsa/pcm_external.h>
#include <alsa/pcm_plugin.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cvi_ain.h"

typedef struct wav_header {
    char chunkId[4]; // "RIFF"
    uint32_t chunkSize; // Size of the entire file minus 8 bytes (size of this header)
    char format[4]; // "WAVE"
    char subChunk1Id[4]; // "fmt "
    uint32_t subChunk1Size; // Size of the fmt chunk (usually 16 for PCM)
    uint16_t audioFormat; // Audio format (1 for PCM)
    uint16_t numChannels; // Number of channels (mono, stereo, etc.)
    uint32_t sampleRate; // Sample rate (samples per second)
    uint32_t byteRate; // Byte rate (sampleRate * numChannels * bitsPerSample/8)
    uint16_t blockAlign; // Block align (numChannels * bitsPerSample/8)
    uint16_t bitsPerSample; // Bits per sample (8, 16, 24, etc.)
    char subChunk2Id[4]; // "data"
    uint32_t subChunk2Size; // Size of the data section
} wav_header_t;

typedef struct cvi_pcm_plugin {
    snd_pcm_ioplug_t io;
    FILE* audio_file;
    wav_header_t header;
    snd_pcm_format_t format;
    unsigned int channels;
    unsigned int rate;
    snd_pcm_sframes_t frames_count;
} cvi_pcm_plugin_t;

static int read_wav_header(cvi_pcm_plugin_t* plugin)
{
    wav_header_t* header = &plugin->header;
    size_t result;

    printf("%s,%d\n", __FUNCTION__, __LINE__);
    result = fread(header, sizeof(wav_header_t), 1, plugin->audio_file);
    if (result != 1) {
        fprintf(stderr, "Failed to read WAV header\n");
        return -1;
    }

    switch (header->bitsPerSample) {
    case 16:
        plugin->format = SND_PCM_FORMAT_S16_LE;
        break;
    default:
        fprintf(stderr, "Unsupported bits per sample: %d\n", header->bitsPerSample);
        return -1;
    }

    plugin->channels = header->numChannels;
    plugin->rate = header->sampleRate;

    printf("Format: %d\n", plugin->format);
    printf("Channels: %d\n", plugin->channels);
    printf("Rate: %d\n", plugin->rate);

    return 0;
}

static int init(cvi_pcm_plugin_t* plugin, const char* filename)
{
    printf("%s,%d\n", __FUNCTION__, __LINE__);
    if (!plugin->audio_file) {
        plugin->audio_file = fopen(filename, "rb");
        if (!plugin->audio_file) {
            perror("Failed to open audio file");
            return -1;
        }
    }

    printf("%s,%d\n", __FUNCTION__, __LINE__);
    if (read_wav_header(plugin)) {
        fclose(plugin->audio_file);
        return -1;
    }

    plugin->frames_count = 44100;

    return 0;
}

static int start(snd_pcm_ioplug_t* io)
{
    printf("%s,%d\n", __FUNCTION__, __LINE__);
    // return snd_pcm_start(io->pcm);
    return 0;
}

static int stop(snd_pcm_ioplug_t* io)
{
    printf("%s,%d\n", __FUNCTION__, __LINE__);
    // return snd_pcm_drop(io->pcm);
    return 0;
}

static snd_pcm_sframes_t pointer(snd_pcm_ioplug_t* io)
{
    cvi_pcm_plugin_t* plugin = (cvi_pcm_plugin_t*)io;

    printf("%s,%d: +++++++++++++++++++++++++\n", __FUNCTION__, __LINE__);
    printf("%s,%d: appl_ptr=%d, hw_ptr=%d, frames_count=%d\n", __FUNCTION__, __LINE__,
        io->appl_ptr, io->hw_ptr, plugin->frames_count);
    printf("%s,%d: +++++++++++++++++++++++++\n", __FUNCTION__, __LINE__);

    return plugin->frames_count;
}

// 数据传输回调函数
static snd_pcm_sframes_t transfer(snd_pcm_ioplug_t* io, const snd_pcm_channel_area_t* areas,
    snd_pcm_uframes_t offset, snd_pcm_uframes_t size)
{
    // printf("%s,%d: offset=%d, size=%d\n", __FUNCTION__, __LINE__, offset, size);
    cvi_pcm_plugin_t* plugin = (cvi_pcm_plugin_t*)io;
    size_t bytes_per_frame = snd_pcm_format_width(plugin->format) / 8 * plugin->channels;
    size_t total_bytes = size * bytes_per_frame;

    sleep(1);

    // 从文件读取音频数据
    size_t read_bytes = fread((char*)areas[0].addr + areas[0].first / 8 + offset * bytes_per_frame,
        1, total_bytes, plugin->audio_file);

    // 更新已读取的帧数
    plugin->frames_count += read_bytes / bytes_per_frame;

    printf("%s,%d: >>>>>>>>>>>>>>>>>>>>>>>>>\n", __FUNCTION__, __LINE__);
    printf("%s,%d: offset=%d, frm_size=%d, read_bytes=%d(%d) -> %d\n", __FUNCTION__, __LINE__,
        offset, size, read_bytes, total_bytes, read_bytes / bytes_per_frame);
    printf("%s,%d: <<<<<<<<<<<<<<<<<<<<<<<<<\n", __FUNCTION__, __LINE__);
    // 如果读取的数据少于请求的数据量，则填充剩余部分为静音
    if (read_bytes < total_bytes) {
        memset((char*)areas[0].addr + areas[0].first / 8 + offset * bytes_per_frame + read_bytes,
            0, total_bytes - read_bytes);
        return read_bytes / bytes_per_frame;
    }

    return size;
}

static int close_cb(snd_pcm_ioplug_t* io)
{
    printf("%s,%d\n", __FUNCTION__, __LINE__);
    cvi_pcm_plugin_t* plugin = (cvi_pcm_plugin_t*)io;
    if (plugin->audio_file) {
        fclose(plugin->audio_file);
        plugin->audio_file = NULL;
    }
    free(plugin);

    return 0;
}

SND_PCM_PLUGIN_DEFINE_FUNC(cvi_audio)
{
    int err = 0;

    printf("%s,%d: %s\n", __FUNCTION__, __LINE__, __TIME__);
    cvi_pcm_plugin_t* plugin = NULL;
    const char* file_name = NULL;
    snd_config_iterator_t i, next;

    printf("%s,%d\n", __FUNCTION__, __LINE__);
    plugin = calloc(1, sizeof(*plugin));
    if (!plugin) {
        SNDERR("Failed to allocate memory for plugin");
        return -ENOMEM;
    }

    printf("%s,%d\n", __FUNCTION__, __LINE__);
    snd_config_for_each(i, next, conf)
    {
        snd_config_t* n = snd_config_iterator_entry(i);
        const char* id;

        if (snd_config_get_id(n, &id) < 0)
            continue;

        if (strcmp(id, "comment") == 0 || strcmp(id, "type") == 0 || strcmp(id, "hint") == 0)
            continue;

        if (strcmp(id, "file") == 0) {
            if (snd_config_get_string(n, &file_name) < 0) {
                SNDERR("Invalid type for %s", id);
                err = -EINVAL;
                goto exit;
            }
        } else {
            SNDERR("Unknown field %s", id);
            err = -EINVAL;
            goto exit;
        }
    }

    printf("file_name: %s\n", file_name);
    if (init(plugin, file_name)) {
        err = -EINVAL;
        goto exit;
    }

    static const snd_pcm_ioplug_callback_t callback = {
        .start = start,
        .stop = stop,
        .pointer = pointer,
        .transfer = transfer,
        .close = close_cb,
    };

    snd_pcm_ioplug_t* io = &plugin->io;

    io->version = SND_PCM_IOPLUG_VERSION;
    io->name = "cvi_audio";
    io->flags = SND_PCM_IOPLUG_FLAG_MONOTONIC;
    io->mmap_rw = 0;
    io->callback = &callback;
    io->private_data = plugin;

    err = snd_pcm_ioplug_create(&plugin->io, name, SND_PCM_STREAM_CAPTURE, mode);
    printf("%s,%d: name=%s, mode=%d\n", __FUNCTION__, __LINE__, name, mode);
    if (err < 0) {
        SNDERR("snd_pcm_ioplug_create, err=%d", err);
        goto exit;
    }

    *pcmp = plugin->io.pcm;

    printf("%s,%d: SSSSSSSSSSSSSSSSSSSS\n", __FUNCTION__, __LINE__);
    printf("%s,%d: *pcmp=%p\n", __FUNCTION__, __LINE__, *pcmp);
    printf("%s,%d: buffer_size=%d\n", __FUNCTION__, __LINE__, plugin->io.buffer_size);
    printf("%s,%d: EEEEEEEEEEEEEEEEEEEE\n", __FUNCTION__, __LINE__);

    snd_pcm_sframes_t pcm_avail = snd_pcm_ioplug_hw_avail(io, io->hw_ptr, io->appl_ptr);
    printf("%s,%d: +++++++++++++++++++++++++\n", __FUNCTION__, __LINE__);
    printf("%s,%d: frames_count=%d\n", __FUNCTION__, __LINE__, plugin->frames_count);
    printf("%s,%d: stream=%d\n", __FUNCTION__, __LINE__, io->stream);
    printf("%s,%d: state=%d\n", __FUNCTION__, __LINE__, io->state);
    printf("%s,%d: appl_ptr=%d\n", __FUNCTION__, __LINE__, io->appl_ptr);
    printf("%s,%d: hw_ptr=%d\n", __FUNCTION__, __LINE__, io->hw_ptr);
    printf("%s,%d: pcm_avail=%d\n", __FUNCTION__, __LINE__, pcm_avail);
    printf("%s,%d: +++++++++++++++++++++++++\n", __FUNCTION__, __LINE__);

    return 0;

exit:
    if (plugin) {
        free(plugin);
    }

    return err;
}

SND_PCM_PLUGIN_SYMBOL(cvi_audio);
