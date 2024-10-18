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
    bool dropped;
    std::vector<MessageBox*> msgboxes;
} channel;

class videoFrame {
public:
    videoFrame() : ref_cnt(0) {
        memset(&img, 0, sizeof(ma_img_t));
    }
    ~videoFrame() = default;
    inline void ref(int n = 1) {
        ref_cnt.fetch_add(n, std::memory_order_relaxed);
    }
    inline void release() {
        if (ref_cnt.load(std::memory_order_relaxed) == 0 || ref_cnt.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            if (!img.physical) {
                delete[] img.data;
            }
            delete this;
        }
    }
    ma_tick_t timestamp;
    std::atomic<int> ref_cnt;
    int8_t fps;
    ma_img_t img;
};

class CameraNode : public Node {

public:
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
    int vencCallback(void* pData, void* pArgs);
    int vpssCallback(void* pData, void* pArgs);
    static int vencCallbackStub(void* pData, void* pArgs, void* pUserData);
    static int vpssCallbackStub(void* pData, void* pArgs, void* pUserData);
    std::vector<channel> channels_;
    int option_;
};

}  // namespace ma::node