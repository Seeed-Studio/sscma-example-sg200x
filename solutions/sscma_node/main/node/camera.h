#pragma once

#include "node.h"
#include "server.h"

#include "video.h"

namespace ma::node {

enum { CHN_RAW = 0, CHN_JPEG = 1, CHN_H264 = 2, CHN_MAX };

typedef struct {
    int32_t width;
    int32_t height;
    int32_t fps;
    ma_pixel_format_t format;
    bool configured;
    bool enabled;
    std::vector<MessageBox*> msgboxes;
} channel;

class videoFrame {
public:
    videoFrame(bool own = true) : ref_cnt(0), sem(0), own(own) {
        memset(&img, 0, sizeof(ma_img_t));
    }
    ~videoFrame() = default;
    inline void ref(int n = 1) {
        ref_cnt.fetch_add(n, std::memory_order_relaxed);
    }
    inline void wait() {
        sem.wait();
    }
    inline void release() {
        if (ref_cnt.load(std::memory_order_relaxed) == 0 ||
            ref_cnt.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            if (own) {
                delete[] img.data;
                delete this;
            } else {
                sem.signal();
            }
        }
    }
    Semaphore sem;
    ma_tick_t timestamp;
    std::atomic<int> ref_cnt;
    bool own;
    bool physical;
    uint32_t index;
    uint32_t count;
    uint64_t phy_addr;
    ma_img_t img;
};

class CameraNode : public Node {

public:
    CameraNode(std::string id);
    ~CameraNode();

    ma_err_t onCreate(const json& config) override;
    ma_err_t onStart() override;
    ma_err_t onMessage(const json& message) override;
    ma_err_t onStop() override;
    ma_err_t onDestroy() override;

    ma_err_t config(int chn,
                    int32_t width            = -1,
                    int32_t height           = -1,
                    int32_t fps              = -1,
                    ma_pixel_format_t format = MA_PIXEL_FORMAT_UNKNOWN,
                    bool enabled             = true);
    ma_err_t attach(int chn, MessageBox* msgbox);
    ma_err_t detach(int chn, MessageBox* msgbox);


protected:
    int vencCallback(void* pData, void* pArgs);
    int vpssCallback(void* pData, void* pArgs);
    static int vencCallbackStub(void* pData, void* pArgs, void* pUserData);
    static int vpssCallbackStub(void* pData, void* pArgs, void* pUserData);
    std::vector<channel> channels_;
    std::atomic<bool> started_;
};

}  // namespace ma::node