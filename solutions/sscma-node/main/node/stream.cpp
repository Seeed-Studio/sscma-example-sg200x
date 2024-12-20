#include <alsa/asoundlib.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

#include "stream.h"

namespace ma::node {

static constexpr char TAG[] = "ma::node::stream";

StreamNode::StreamNode(std::string id) : Node("stream", id), port_(0), host_(""), url_(""), username_(""), password_(""), thread_(nullptr), camera_(nullptr), frame_(30), transport_(nullptr) {
    char hostname[1024];
    hostname[1023] = '\0';
    gethostname(hostname, 1023);
    host_ = hostname;
}

StreamNode::~StreamNode() {
    onDestroy();
}

void StreamNode::threadEntry() {
    videoFrame* frame = nullptr;

    while (started_) {
        if (frame_.fetch(reinterpret_cast<void**>(&frame), Tick::fromSeconds(2))) {
            Thread::enterCritical();
            for (auto& block : frame->blocks) {
                transport_->send(reinterpret_cast<const char*>(block.first), block.second);
            }
            frame->release();
            Thread::exitCritical();
        }
    }
}

void StreamNode::threadEntryStub(void* obj) {
    reinterpret_cast<StreamNode*>(obj)->threadEntry();
}

void StreamNode::threadAudioEntry() {
    const char* device_name = "hw:0";
    snd_pcm_t* handle;
    snd_pcm_hw_params_t* params;
    snd_pcm_uframes_t frames;
    int pcm_return;
    uint16_t* buffer;
    int rate            = 16000;                 // 采样率
    int channels        = 1;                     // 声道数（立体声）
    int bits_per_sample = 16;                    // 每个样本 16 位
    int buffer_size     = 16000 * 2 * channels;  // 16000 * 2 字节/通道 * 2 通道）

    // 打开 PCM 设备
    pcm_return = snd_pcm_open(&handle, device_name, SND_PCM_STREAM_CAPTURE, 0);
    if (pcm_return < 0) {
        std::cerr << "Unable to open PCM device " << device_name << std::endl;
        return;
    }
    std::cout << "Opened PCM device " << device_name << std::endl;

    // 设置硬件参数
    snd_pcm_hw_params_malloc(&params);
    snd_pcm_hw_params_any(handle, params);
    snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_rate(handle, params, rate, 0);
    snd_pcm_hw_params_set_channels(handle, params, channels);
    snd_pcm_hw_params(handle, params);
    snd_pcm_hw_params_free(params);

    // 分配缓冲区
    frames = 640;
    buffer = new uint16_t[frames * channels];


    // 开始录音
    while (started_) {
        pcm_return = snd_pcm_readi(handle, buffer, frames);
        if (pcm_return == -EPIPE) {
            std::cerr << "Buffer overrun occurred" << std::endl;
            snd_pcm_prepare(handle);
        } else if (pcm_return < 0) {
            std::cerr << "Error reading from PCM device: " << snd_strerror(pcm_return) << std::endl;
            break;
        }
        transport_->sendAudio(reinterpret_cast<char*>(buffer), pcm_return * channels * (bits_per_sample / 8));
    }
    // 停止录音
    snd_pcm_close(handle);
    delete[] buffer;
}
void StreamNode::threadAudioEntryStub(void* obj) {
    reinterpret_cast<StreamNode*>(obj)->threadAudioEntry();
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

    audio_thread_ = new Thread((type_ + "#" + id_ + "#audio").c_str(), threadAudioEntryStub);
    if (audio_thread_ == nullptr) {
        MA_THROW(Exception(MA_ENOMEM, "Not enough memory"));
    }

    // show url
    MA_LOGI(TAG, "%s", url_.c_str());

    transport_ = new TransportRTSP();

    if (transport_ == nullptr) {
        MA_THROW(Exception(MA_ENOMEM, "Not enough memory"));
    }

    TransportRTSP::Config rtspConfig = {port_, MA_PIXEL_FORMAT_H264, MA_AUDIO_FORMAT_PCM, 16000, 1, 16, session_, username_, password_};
    err                              = transport_->init(&rtspConfig);
    if (err != MA_OK) {
        MA_THROW(Exception(err, "RTSP transport init failed"));
    }
    server_->response(id_, json::object({{"type", MA_MSG_TYPE_RESP}, {"name", "create"}, {"code", err}, {"data", {"url", url_}}}));
    created_ = true;
    return err;
}

ma_err_t StreamNode::onControl(const std::string& control, const json& data) {
    Guard guard(mutex_);
    server_->response(id_, json::object({{"type", MA_MSG_TYPE_RESP}, {"name", control}, {"code", MA_ENOTSUP}, {"data", "not supported"}}));
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

    if (audio_thread_ != nullptr) {
        delete audio_thread_;
        audio_thread_ = nullptr;
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

    started_ = true;

    thread_->start(this);
    audio_thread_->start(this);

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

    if (audio_thread_ != nullptr) {
        printf("audio_thread_->join()\n");
        audio_thread_->join();
        printf("audio_thread_->join() done\n");
    }

    if (camera_ != nullptr) {
        camera_->detach(CHN_H264, &frame_);
    }

    if (transport_ != nullptr) {
        transport_->deInit();
    }
    return MA_OK;
}

REGISTER_NODE("stream", StreamNode);

}  // namespace ma::node