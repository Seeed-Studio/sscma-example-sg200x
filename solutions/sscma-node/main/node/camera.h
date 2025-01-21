#pragma once

#include "node.h"
#include "server.h"

#include "video.h"

namespace ma::node {

#define AUDIO_DEVICE "hw:0"
#define SAMPLE_RATE  16000
#define CHANNELS     1
#define FORMAT       SND_PCM_FORMAT_S16_LE

enum { CHN_RAW = 0, CHN_JPEG = 1, CHN_H264 = 2, CHN_AUDIO = 3, CHN_MAX };

typedef struct {
    int chn;
    int32_t width;
    int32_t height;
    int32_t fps;
    ma_pixel_format_t format;
    bool configured;
    bool enabled;
    bool dropped;
    std::vector<MessageBox*> msgboxes;
} channel;

class Frame {
public:
    Frame() : ref_cnt(0), chn(CHN_MAX) {}
    ~Frame() = default;
    inline void ref(int n = 1) {
        ref_cnt.fetch_add(n, std::memory_order_relaxed);
    }
    virtual inline void release() = 0;
    int chn;
    std::atomic<int> ref_cnt;
    ma_tick_t timestamp;
};

class videoFrame : public Frame {
public:
    videoFrame() : Frame() {
        memset(&img, 0, sizeof(img));
    }
    inline void release() override {
        if (ref_cnt.load(std::memory_order_relaxed) == 0 || ref_cnt.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            if (!img.physical) {
                delete[] img.data;
            }
            delete this;
        }
    }
    std::vector<std::pair<void*, size_t>> blocks;
    ma_img_t img;
    int fps;
};

class audioFrame : public Frame {
public:
    audioFrame() : Frame() {
        data = nullptr;
        size = 0;
    }
    inline void release() override {
        if (ref_cnt.load(std::memory_order_relaxed) == 0 || ref_cnt.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            delete[] data;
            delete this;
        }
    }

    uint8_t* data;
    size_t size;
};

class CameraNode : public Node {

public:
    using audioCallback = void (*)(const uint8_t* data, size_t size);
    CameraNode(std::string id);
    ~CameraNode();

    ma_err_t onCreate(const json& config) override;
    ma_err_t onStart() override;
    ma_err_t onControl(const std::string& control, const json& data) override;
    ma_err_t onStop() override;
    ma_err_t onDestroy() override;

    ma_err_t config(int chn, int32_t width = -1, int32_t height = -1, int32_t fps = -1, ma_pixel_format_t format = MA_PIXEL_FORMAT_UNKNOWN, bool enabled = true);
    ma_err_t attach(int chn, MessageBox* msgbox);
    ma_err_t detach(int chn, MessageBox* msgbox);

protected:
    void threadEntry();
    void threadAudioEntry();
    static void threadEntryStub(void* obj);
    static void threadAudioEntryStub(void* obj);
    int vencCallback(void* pData, void* pArgs);
    int vpssCallback(void* pData, void* pArgs);
    static int vencCallbackStub(void* pData, void* pArgs, void* pUserData);
    static int vpssCallbackStub(void* pData, void* pArgs, void* pUserData);

private:
    std::vector<channel> channels_;
    uint32_t count_;
    bool preview_;
    bool websocket_;
    int audio_;
    int option_;
    int light_;
    Thread* thread_;
    Thread* thread_audio_;
    MessageBox frame_;
    TransportWebSocket* transport_;
};

}  // namespace ma::node