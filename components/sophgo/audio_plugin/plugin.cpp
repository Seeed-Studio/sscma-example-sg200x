#include <alsa/asoundlib.h>
#include <alsa/pcm_external.h>
#include <alsa/pcm_plugin.h>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>

// WAV文件头结构体
struct WAVHeader {
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
};

// 插件结构体
struct MyPcmPlugin {
    snd_pcm_ioplug_t io;
    // snd_pcm_hw_params_t* hw_params;
    std::unique_ptr<std::ifstream> audio_file;
    snd_pcm_format_t format;
    unsigned int channels;
    unsigned int rate;

    // 构造函数
    MyPcmPlugin()
        : audio_file(std::make_unique<std::ifstream>())
    {
    }

    // 析构函数
    // ~MyPcmPlugin()
    // {
    //     if (hw_params) {
    //         snd_pcm_hw_params_free(hw_params);
    //         hw_params = nullptr;
    //     }
    // }

    // 初始化插件
    int init(const char* filename)
    {
        if (!readWavHeader(filename)) {
            return -1;
        }

        // 设置硬件参数
        // io.hw.formats = snd_pcm_format_mask_malloc();
        // snd_pcm_format_mask_set(io.hw.formats, format);
        // io.hw.channels_min = io.hw.channels_max = channels;
        // io.hw.rates = snd_pcm_rate_linear(hw_params_get_rate_min(nullptr), hw_params_get_rate_max(nullptr));
        // io.hw.rate_min = io.hw.rate_max = rate;
        // io.hw.buffer_size_min = io.hw.buffer_size_max = 1024;
        // io.hw.period_size_min = io.hw.period_size_max = 512;

        // snd_pcm_hw_params_malloc(&hw_params);

        return 0;
    }

    // 读取WAV文件头信息
    bool readWavHeader(const std::string& filename)
    {
        WAVHeader header;

        audio_file->open(filename, std::ios::binary);
        if (!audio_file->is_open()) {
            std::cerr << "Failed to open file: " << filename << std::endl;
            return false;
        }

        audio_file->read(reinterpret_cast<char*>(&header), sizeof(WAVHeader));

        switch (header.bitsPerSample) {
        case 16:
            format = SND_PCM_FORMAT_S16_LE;
            break;
        default:
            std::cerr << "Unsupported bits per sample: " << header.bitsPerSample << std::endl;
            return false;
        }

        channels = header.numChannels;
        rate = header.sampleRate;

        return true;
    }

    // 数据传输回调函数
    static snd_pcm_sframes_t transfer(snd_pcm_ioplug_t* io, const snd_pcm_channel_area_t* areas,
        snd_pcm_uframes_t offset, snd_pcm_uframes_t size)
    {
        MyPcmPlugin* plugin = reinterpret_cast<MyPcmPlugin*>(io);
        size_t bytes_per_frame = snd_pcm_format_width(plugin->format) / 8 * plugin->channels;
        size_t total_bytes = size * bytes_per_frame;

        // 从文件读取音频数据
        plugin->audio_file->read((char*)areas[0].addr + areas[0].first / 8 + offset * bytes_per_frame,
            total_bytes);

        // 获取实际读取的字节数
        size_t read_bytes = plugin->audio_file->gcount();

        // 如果读取的数据少于请求的数据量，则填充剩余部分为静音
        if (read_bytes < total_bytes) {
            memset((char*)areas[0].addr + areas[0].first / 8 + offset * bytes_per_frame + read_bytes,
                0, total_bytes - read_bytes);
            return read_bytes / bytes_per_frame;
        }

        return size;
    }

    // 插件关闭回调函数
    static int close_cb(snd_pcm_ioplug_t* io)
    {
        MyPcmPlugin* plugin = reinterpret_cast<MyPcmPlugin*>(io);
        if (plugin->audio_file) {
            plugin->audio_file->close();
        }

        return 0;
    }
};

// 插件初始化函数
SND_PCM_PLUGIN_DEFINE_FUNC(cvi_audio)
{
    MyPcmPlugin* plugin = nullptr;
    snd_config_iterator_t i, next;
    char file_name[256] = "";

    try {
        plugin = new MyPcmPlugin();
        snd_config_for_each(i, next, conf)
        {
            snd_config_t* n = snd_config_iterator_entry(i);
            const char* id;
            if (snd_config_get_id(n, &id) < 0)
                continue;
            if (strcmp(id, "comment") == 0 || strcmp(id, "type") == 0 || strcmp(id, "hint") == 0)
                continue;
            if (strcmp(id, "file") == 0) {
                const char* file;
                if (snd_config_get_string(n, &file) < 0) {
                    SNDERR("Invalid type for %s", id);
                    delete plugin;
                    return -EINVAL;
                }
                strcpy(file_name, file);

                if (plugin->init(file) != 0) {
                    delete plugin;
                    return -EINVAL;
                }
            } else {
                SNDERR("Unknown field %s", id);
                delete plugin;
                return -EINVAL;
            }
        }
        std::cout << "Plugin name: " << file_name << std::endl;

        const snd_pcm_ioplug_callback_t callback = {
            .transfer = MyPcmPlugin::transfer,
            .close = MyPcmPlugin::close_cb,
        };

        plugin->io.version = SND_PCM_IOPLUG_VERSION;
        plugin->io.name = name;
        plugin->io.callback = &callback;
        plugin->io.private_data = plugin;
        snd_pcm_ioplug_create(&plugin->io, name, SND_PCM_STREAM_CAPTURE, 0);

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Exception caught: " << e.what() << std::endl;
        if (plugin) {
            delete plugin;
        }
        return -EINVAL;
    }
}

// struct snd_dlsym_link *snd_dlsym_start;
SND_PCM_PLUGIN_SYMBOL(cvi_audio);
