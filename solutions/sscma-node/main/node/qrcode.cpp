// qrcode.cpp
#include <cstring>
#include <unistd.h>

#include <opencv2/opencv.hpp>  // Correct OpenCV include

#include "model.h"  // for videoFrame, ma_img_t, etc.
#include "qrcode.h"

namespace ma::node {

using namespace ma::engine;
using namespace ma::model;

static constexpr char TAG[] = "ma::node::qrcode";

QRCodeNode::QRCodeNode(std::string id) : Node("qr", id), count_(0), thread_(nullptr), camera_(nullptr), raw_frame_(1) {}

QRCodeNode::~QRCodeNode() {
    onDestroy();
}

void QRCodeNode::threadEntry() {
    videoFrame* frame = nullptr;

    // Notify initial enabled state
    server_->response(id_, json::object({{"type", MA_MSG_TYPE_RESP}, {"name", "enabled"}, {"code", MA_OK}, {"data", enabled_.load()}}));

    while (started_) {
        // Fetch raw frame with timeout
        if (!raw_frame_.fetch(reinterpret_cast<void**>(&frame), Tick::fromSeconds(2))) {
            MA_LOGW(TAG, "Frame fetch timeout");
            continue;
        }

        // Skip if disabled
        if (!enabled_) {
            frame->release();
            continue;
        }

        Thread::enterCritical();
        struct quirc* q = nullptr;
        try {
            const int width     = frame->img.width;
            const int height    = frame->img.height;
            const uint8_t* data = nullptr;
            if (frame->img.physical) {
                data = static_cast<uint8_t*>(CVI_SYS_Mmap(reinterpret_cast<CVI_U64>(frame->img.data), frame->img.size));
            } else {
                data = frame->img.data;
            }

            // Initialize quirc decoder
            q = quirc_new();
            if (!q) {
                MA_LOGE(TAG, "Failed to allocate quirc decoder");
                if (frame->img.physical) {
                    CVI_SYS_Munmap(frame->img.data, frame->img.size);
                }
                frame->release();
                Thread::exitCritical();
                continue;
            }

            if (quirc_resize(q, width, height) < 0) {
                MA_LOGE(TAG, "Failed to resize quirc decoder");
                if (frame->img.physical) {
                    CVI_SYS_Munmap(frame->img.data, frame->img.size);
                }
                frame->release();
                quirc_destroy(q);
                Thread::exitCritical();
                continue;
            }

            // Convert RGB888 -> Grayscale (required by quirc)
            uint8_t* gray_image = quirc_begin(q, nullptr, nullptr);

            for (int i = 0; i < width * height; i++) {
                uint8_t r     = data[i * 3 + 0];
                uint8_t g     = data[i * 3 + 1];
                uint8_t b     = data[i * 3 + 2];
                gray_image[i] = (uint8_t)(0.299f * r + 0.587f * g + 0.114f * b);
            }
            quirc_end(q);
            if (frame->img.physical) {
                CVI_SYS_Munmap(frame->img.data, frame->img.size);
            }

            // Decode all QR codes
            json result_data          = json::object();
            result_data["count"]      = quirc_count(q);
            result_data["codes"]      = json::array();
            result_data["resolution"] = json::array({width, height});

            for (int i = 0; i < quirc_count(q); ++i) {
                struct quirc_code code;
                struct quirc_data data_out;
                quirc_extract(q, i, &code);

                if (quirc_decode(&code, &data_out) == QUIRC_SUCCESS) {
                    json qr_code         = json::object();
                    qr_code["payload"]   = std::string(reinterpret_cast<char*>(data_out.payload), data_out.payload_len);
                    qr_code["ecc_level"] = data_out.ecc_level;
                    qr_code["mask"]      = data_out.mask;
                    qr_code["version"]   = data_out.version;

                    json corners = json::array();
                    for (int k = 0; k < 4; ++k) {
                        corners.push_back({static_cast<int16_t>(code.corners[k].x), static_cast<int16_t>(code.corners[k].y)});
                    }
                    qr_code["corners"] = corners;

                    result_data["codes"].push_back(qr_code);
                }
            }

            // Send result
            server_->response(id_, json::object({{"type", MA_MSG_TYPE_EVT}, {"name", "qrcode"}, {"code", MA_OK}, {"data", result_data}}));

        } catch (const std::exception& e) {
            MA_LOGE(TAG, "Error processing frame: %s", e.what());
        }

        // Cleanup
        if (q) {
            quirc_destroy(q);
        }
        frame->release();
        Thread::sleep(Tick::fromMilliseconds(500));  // limit fps
        Thread::exitCritical();
    }
}

void QRCodeNode::threadEntryStub(void* obj) {
    reinterpret_cast<QRCodeNode*>(obj)->threadEntry();
}

ma_err_t QRCodeNode::onCreate(const json& config) {
    Guard guard(mutex_);

    thread_ = new Thread(("qr#" + id_).c_str(), &QRCodeNode::threadEntryStub, this);
    if (!thread_) {
        MA_THROW(Exception(MA_ENOMEM, "Failed to create QR worker thread"));
    }

    created_ = true;
    server_->response(id_, json::object({{"type", MA_MSG_TYPE_RESP}, {"name", "create"}, {"code", MA_OK}, {"data", json::object({{"status", "initialized"}})}}));

    return MA_OK;
}

ma_err_t QRCodeNode::onStart() {
    Guard guard(mutex_);
    if (started_)
        return MA_OK;

    // Find connected camera node
    camera_ = nullptr;
    for (auto& dep : dependencies_) {
        if (dep.second->type() == "camera") {
            camera_ = static_cast<CameraNode*>(dep.second);
            break;
        }
    }

    if (!camera_) {
        MA_THROW(Exception(MA_ENOTSUP, "No camera node found"));
    }


    camera_->config(CHN_RAW);
    camera_->attach(CHN_RAW, &raw_frame_);

    started_ = true;
    thread_->start(this);

    MA_LOGI(TAG, "QRCodeNode started: %s ", id_.c_str());

    return MA_OK;
}

ma_err_t QRCodeNode::onStop() {
    Guard guard(mutex_);
    if (!started_)
        return MA_OK;

    started_ = false;
    if (thread_) {
        thread_->join();
    }

    if (camera_) {
        camera_->detach(CHN_RAW, &raw_frame_);
        camera_ = nullptr;
    }

    return MA_OK;
}

ma_err_t QRCodeNode::onControl(const std::string& control, const json& data) {
    Guard guard(mutex_);

    if (control == "enabled" && data.is_boolean()) {
        enabled_.store(data.get<bool>());
        server_->response(id_, json::object({{"type", MA_MSG_TYPE_RESP}, {"name", control}, {"code", MA_OK}, {"data", enabled_.load()}}));
    } else if (control == "config") {
        server_->response(id_, json::object({{"type", MA_MSG_TYPE_RESP}, {"name", control}, {"code", MA_OK}, {"data", data}}));
    } else {
        server_->response(id_, json::object({{"type", MA_MSG_TYPE_RESP}, {"name", control}, {"code", MA_ENOTSUP}, {"data", "Unsupported control"}}));
    }

    return MA_OK;
}

ma_err_t QRCodeNode::onDestroy() {
    Guard guard(mutex_);
    if (!created_)
        return MA_OK;

    onStop();

    if (thread_) {
        delete thread_;
        thread_ = nullptr;
    }

    created_ = false;
    return MA_OK;
}

REGISTER_NODE_SINGLETON("qr", QRCodeNode);

}  // namespace ma::node