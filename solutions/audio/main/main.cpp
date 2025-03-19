#include <alsa/asoundlib.h>
#include <cstring>
#include <fstream>
#include <iostream>

#define SAMPLE_RATE 16000
#define CHANNELS    1
#define FORMAT      SND_PCM_FORMAT_S16_LE
#define DURATION    5

static snd_pcm_t* handle;
static snd_pcm_uframes_t chunk_size    = 0;
static unsigned period_time            = 0;
static unsigned buffer_time            = 0;
static snd_pcm_uframes_t period_frames = 0;
static snd_pcm_uframes_t buffer_frames = 0;
static int avail_min                   = -1;
static int start_delay                 = 0;
static int stop_delay                  = 0;
static size_t significant_bits_per_sample, bits_per_sample, bits_per_frame;
static size_t chunk_bytes;
static u_char* audiobuf             = NULL;
static snd_pcm_chmap_t* channel_map = NULL; /* chmap to override */
static unsigned int* hw_map         = NULL; /* chmap to follow */
static snd_output_t* log;

// WAV 文件头结构
struct WAVHeader {
    char riff[4];                    // "RIFF"
    unsigned int size;               // 4 + (8 + size of subchunks)
    char wave[4];                    // "WAVE"
    char fmt[4];                     // "fmt "
    unsigned int fmt_size;           // 16 for PCM
    unsigned short audio_format;     // 1 for PCM
    unsigned short num_channels;     // 1 or 2
    unsigned int sample_rate;        // 16000
    unsigned int byte_rate;          // sample_rate * num_channels * bytes_per_sample
    unsigned short block_align;      // num_channels * bytes_per_sample
    unsigned short bits_per_sample;  // 16
    char data[4];                    // "data"
    unsigned int data_size;          // num_samples * num_channels * bytes_per_sample
};

// WAV 文件写入
void write_wav_header(std::ofstream& file, unsigned int data_size) {
    WAVHeader header;
    std::memset(&header, 0, sizeof(header));

    std::strcpy(header.riff, "RIFF");
    header.size = 36 + data_size;
    std::strcpy(header.wave, "WAVE");
    std::strcpy(header.fmt, "fmt ");
    header.fmt_size        = 16;
    header.audio_format    = 1;  // PCM
    header.num_channels    = CHANNELS;
    header.sample_rate     = SAMPLE_RATE;
    header.byte_rate       = SAMPLE_RATE * CHANNELS * 2;  // 16-bit (2 bytes)
    header.block_align     = CHANNELS * 2;
    header.bits_per_sample = 16;
    std::strcpy(header.data, "data");
    header.data_size = data_size;

    file.write(reinterpret_cast<char*>(&header), sizeof(header));
}


/*
 *	Subroutine to clean up before exit.
 */
static void prg_exit(int code) {
    if (handle)
        snd_pcm_close(handle);

    exit(code);
}

static void set_params(snd_pcm_t* handle) {
    snd_pcm_hw_params_t* params;
    snd_pcm_uframes_t buffer_size;
    int err;
    size_t n;
    unsigned int rate;
    snd_pcm_hw_params_alloca(&params);
    err = snd_pcm_hw_params_any(handle, params);
    if (err < 0) {
        std::cerr << "Error setting hardware parameters: " << snd_strerror(err) << std::endl;
        return;
    }

    err = snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);

    if (err < 0) {
        std::cerr << "Error setting access mode: " << snd_strerror(err) << std::endl;
    }
    err = snd_pcm_hw_params_set_format(handle, params, FORMAT);
    if (err < 0) {
        std::cerr << "Error setting format: " << snd_strerror(err) << std::endl;
    }
    err = snd_pcm_hw_params_set_channels(handle, params, CHANNELS);
    if (err < 0) {
        std::cerr << "Error setting channels: " << snd_strerror(err) << std::endl;
    }

    rate = SAMPLE_RATE;
    err  = snd_pcm_hw_params_set_rate_near(handle, params, &rate, 0);
    assert(err >= 0);
    unsigned int buffer_time = 0;
    if (buffer_time == 0 && buffer_frames == 0) {
        err = snd_pcm_hw_params_get_buffer_time_max(params, &buffer_time, 0);
        assert(err >= 0);
        printf("buffer_time max: %d\n", buffer_time);
        if (buffer_time > 100000)
            buffer_time = 100000;
    }

    printf("buffer_time: %d\n", buffer_time);
    printf("period_time: %d\n", period_time);
    printf("buffer_frames: %d\n", buffer_frames);
    printf("period_frames: %d\n", period_frames);
    // if (period_time == 0 && period_frames == 0) {
    //     if (buffer_time > 0)
            period_time = buffer_time / 4;
    //     else
    //         period_frames = buffer_frames / 4;
    // }
    //if (period_time > 0)
        err = snd_pcm_hw_params_set_period_time_near(handle, params, &period_time, 0);
    // else
    //     err = snd_pcm_hw_params_set_period_size_near(handle, params, &period_frames, 0);
    // assert(err >= 0);
    // if (buffer_time > 0) {
        err = snd_pcm_hw_params_set_buffer_time_near(handle, params, &buffer_time, 0);
    // } else {
    //     err = snd_pcm_hw_params_set_buffer_size_near(handle, params, &buffer_frames);
    // }
    assert(err >= 0);
    err = snd_pcm_hw_params(handle, params);
    if (err < 0) {
        printf("Unable to install hw params:");
        // snd_pcm_hw_params_dump(params, log);
        prg_exit(EXIT_FAILURE);
    }
    snd_pcm_hw_params_get_period_size(params, &chunk_size, 0);
    snd_pcm_hw_params_get_buffer_size(params, &buffer_size);
    if (chunk_size == buffer_size) {
        printf("Can't use period equal to buffer size (%lu == %lu)", chunk_size, buffer_size);
        prg_exit(EXIT_FAILURE);
    }


    bits_per_sample             = snd_pcm_format_physical_width(SND_PCM_FORMAT_S16_LE);
    significant_bits_per_sample = snd_pcm_format_width(SND_PCM_FORMAT_S16_LE);
    bits_per_frame              = bits_per_sample * 2;
    chunk_bytes                 = chunk_size * bits_per_frame / 8;
    audiobuf                    = (u_char*)realloc(audiobuf, chunk_bytes);
    if (audiobuf == NULL) {
        printf("not enough memory");
        prg_exit(EXIT_FAILURE);
    }
    // fprintf(stderr, "real chunk_size = %i, frags = %i, total = %i\n", chunk_size, setup.buf.block.frags, setup.buf.block.frags * chunk_size);

    buffer_frames = buffer_size; /* for position test */
}

int main() {
    const char* device = "hw:0,0";  // 指定设备
    int pcm, size;
    snd_pcm_uframes_t frames = 32;
    short* buffer;
    std::ofstream out_file("test.wav", std::ios::binary);

    pcm = snd_pcm_open(&handle, device, SND_PCM_STREAM_CAPTURE, 0);
    if (pcm < 0) {
        std::cerr << "Error opening PCM device: " << snd_strerror(pcm) << std::endl;
        return -1;
    }

    set_params(handle);

    printf("frames: %d\n", frames);

    write_wav_header(out_file, DURATION * SAMPLE_RATE * CHANNELS * 2);
    frames = chunk_bytes * 8 / bits_per_sample;

    printf("frames: %d\n", frames);

    for (int i = 0; i < (SAMPLE_RATE * DURATION) / frames; ++i) {
        pcm = snd_pcm_readi(handle, audiobuf, frames);
        printf("i: %d %d\n", i, pcm);
        if (pcm < 0) {
            std::cerr << "Error reading from PCM device: " << snd_strerror(pcm) << std::endl;
            break;
        }
        out_file.write(reinterpret_cast<char*>(audiobuf), pcm * 2);  
    }


    snd_pcm_close(handle);
    out_file.close();

    std::cout << "Recording finished, saved to test.wav" << std::endl;
    return 0;
}
