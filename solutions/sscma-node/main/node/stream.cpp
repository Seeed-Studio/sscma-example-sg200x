#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

#include "stream.h"

namespace ma::node {

static constexpr char TAG[] = "ma::node::stream";

StreamNode::StreamNode(std::string id) : Node("stream", id), port_(0), host_(""), url_(""), username_(""), password_(""), thread_(nullptr), camera_(nullptr), frame_(60), transport_(nullptr) {
    char hostname[1024];
    hostname[1023] = '\0';
    gethostname(hostname, 1023);
    host_ = hostname;
}

StreamNode::~StreamNode() {
    onDestroy();
}

void StreamNode::threadEntry() {
    videoFrame* video = nullptr;
    audioFrame* audio = nullptr;
    Frame* frame      = nullptr;

    server_->response(id_, json::object({{"type", MA_MSG_TYPE_RESP}, {"name", "enabled"}, {"code", MA_OK}, {"data", enabled_.load()}}));

    while (started_) {
        if (frame_.fetch(reinterpret_cast<void**>(&frame), Tick::fromSeconds(2))) {
            Thread::enterCritical();
            if (enabled_) {
                if (frame->chn == CHN_H264) {
                    video = static_cast<videoFrame*>(frame);
                    for (auto& block : video->blocks) {
                        transport_->send(reinterpret_cast<const char*>(block.first), block.second);
                    }
                } else if (frame->chn == CHN_AUDIO) {
                    audio = static_cast<audioFrame*>(frame);
                    transport_->sendAudio(reinterpret_cast<const char*>(audio->data), audio->size);
                }
            }
            frame->release();
            Thread::exitCritical();
        }
    }
}

void StreamNode::threadEntryStub(void* obj) {
    reinterpret_cast<StreamNode*>(obj)->threadEntry();
}

ma_err_t StreamNode::onCreate(const json& config) {
    Guard guard(mutex_);
    ma_err_t err = MA_OK;

    if (!config.contains("port")) {
        port_ = 554;
    } else {
        port_ = config["port"].get<int>();
    }
    if (!config.contains("session")) {
        session_ = "live";
    } else {
        session_ = config["session"].get<std::string>();
    }
    if (!config.contains("username")) {
        username_ = "admin";
    } else {
        username_ = config["username"].get<std::string>();
    }
    if (!config.contains("password")) {
        password_ = "admin";
    } else {
        password_ = config["password"].get<std::string>();
    }

    if (session_.empty()) {
        MA_THROW(Exception(MA_EINVAL, "Session is empty"));
    }

    url_ = "rtsp://" + username_ + ":" + password_ + "@" + host_ + ":" + std::to_string(port_) + "/" + session_;

    thread_ = new Thread((type_ + "#" + id_).c_str(), threadEntryStub);

    if (thread_ == nullptr) {
        MA_THROW(Exception(MA_ENOMEM, "Not enough memory"));
    }

    // show url
    MA_LOGI(TAG, "%s", url_.c_str());

    transport_ = new TransportRTSP();

    if (transport_ == nullptr) {
        MA_THROW(Exception(MA_ENOMEM, "Not enough memory"));
    }

    TransportRTSP::Config rtspConfig = {port_, MA_PIXEL_FORMAT_H264, MA_AUDIO_FORMAT_PCM, 16000, 1, 16, session_, username_, password_};

    err = transport_->init(&rtspConfig);
    if (err != MA_OK) {
        MA_THROW(Exception(err, "RTSP transport init failed"));
    }
    server_->response(id_, json::object({{"type", MA_MSG_TYPE_RESP}, {"name", "create"}, {"code", err}, {"data", {"url", url_}}}));
    created_ = true;
    return err;
}

ma_err_t StreamNode::onControl(const std::string& control, const json& data) {
    Guard guard(mutex_);
    if (control == "enabled" && data.is_boolean()) {
        bool enabled = data.get<bool>();
        if (enabled_.load() != enabled) {
            enabled_.store(enabled);
        }
        server_->response(id_, json::object({{"type", MA_MSG_TYPE_RESP}, {"name", control}, {"code", MA_OK}, {"data", enabled_.load()}}));
    } else {
        server_->response(id_, json::object({{"type", MA_MSG_TYPE_RESP}, {"name", control}, {"code", MA_ENOTSUP}, {"data", "Not supported"}}));
    }
    return MA_OK;
}

ma_err_t StreamNode::onDestroy() {

    Guard guard(mutex_);

    if (!created_) {
        return MA_OK;
    }

    onStop();

    if (thread_ != nullptr) {
        delete thread_;
        thread_ = nullptr;
    }

    if (transport_ != nullptr) {
        transport_->deInit();
        delete transport_;
        transport_ = nullptr;
    }

    created_ = false;

    return MA_OK;
}

ma_err_t StreamNode::onStart() {
    ma_err_t err = MA_OK;
    Guard guard(mutex_);
    if (started_) {
        return MA_OK;
    }

    for (auto& dep : dependencies_) {
        if (dep.second->type() == "camera") {
            camera_ = static_cast<CameraNode*>(dep.second);
            break;
        }
    }

    if (camera_ == nullptr) {
        MA_THROW(Exception(MA_ENOTSUP, "No camera node found"));
        return MA_ENOTSUP;
    }

    camera_->config(CHN_H264);
    camera_->attach(CHN_H264, &frame_);
    camera_->attach(CHN_AUDIO, &frame_);

    started_ = true;

    thread_->start(this);

    return MA_OK;
}

ma_err_t StreamNode::onStop() {
    Guard guard(mutex_);
    if (!started_) {
        return MA_OK;
    }

    started_ = false;

    if (thread_ != nullptr) {
        thread_->join();
    }

    if (camera_ != nullptr) {
        camera_->detach(CHN_H264, &frame_);
        camera_->detach(CHN_AUDIO, &frame_);
    }

    if (transport_ != nullptr) {
        transport_->deInit();
    }
    return MA_OK;
}

REGISTER_NODE("stream", StreamNode);

}  // namespace ma::node