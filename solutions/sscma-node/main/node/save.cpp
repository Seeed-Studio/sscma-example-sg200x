#include <filesystem>
#include <sys/statvfs.h>

#include "save.h"

#ifndef NODE_SAVE_PATH_LOCAL
#define NODE_SAVE_PATH_LOCAL "/userdata/VIDEO/"
#endif

#ifndef NODE_SAVE_PATH_EXTERNAL
#define NODE_SAVE_PATH_EXTERNAL "/mnt/sd/VIDEO/"
#endif

#ifndef NODE_SAVE_MAX_SIZE
#define NODE_SAVE_MAX_SIZE 128 * 1024 * 1024
#endif

#ifndef AV_LOG_LEVEL
#define AV_LOG_LEVEL AV_LOG_QUIET
#endif

#define NODE_MIN_AVILABLE_CAPACITY 128 * 1024 * 1024

namespace ma::node {

static constexpr char TAG[] = "ma::node::save";

SaveNode::SaveNode(std::string id)
    : Node("save", id), storage_(NODE_SAVE_PATH_LOCAL), slice_(300), duration_(-1), vcount_(0), acount_(0), camera_(nullptr), frame_(60), thread_(nullptr), avFmtCtx_(nullptr), avStream_(nullptr) {
    // av_log_set_level(AV_LOG_LEVEL);
}

SaveNode::~SaveNode() {
    onDestroy();
}

std::string SaveNode::generateFileName() {
    auto now = std::time(nullptr);
    std::ostringstream oss;
    oss << storage_ << std::put_time(std::localtime(&now), "%Y%m%d_%H%M%S") << ".mov";
    return oss.str();
}

bool SaveNode::recycle(uint32_t req_size) {
    uint64_t avail = std::filesystem::space(storage_).available;
    req_size += NODE_MIN_AVILABLE_CAPACITY;
    if (avail < req_size) {
        std::vector<std::filesystem::directory_entry> files;
        for (const auto& p : std::filesystem::directory_iterator(storage_)) {
            if (p.is_regular_file()) {
                files.push_back(p);
            }
        }
        std::sort(files.begin(), files.end(), [](const auto& a, const auto& b) { return std::filesystem::last_write_time(a) < std::filesystem::last_write_time(b); });
        for (const auto& file : files) {
            if (file.path() == filename_) {
                // skip the current file
                continue;
            }
            uint64_t size = std::filesystem::file_size(file);
            MA_TRY {
                if (std::filesystem::remove(file)) {
                    MA_LOGI(TAG, "recycle %s", file.path().c_str());
                    avail += size;
                    if (avail >= req_size) {
                        break;
                    }
                }
            }
            MA_CATCH(Exception & e) {
                MA_LOGW(TAG, "recycle %s failed: %s", file.path().c_str(), e.what());
                continue;
            }
        }
    }
    return avail >= req_size;
}


bool SaveNode::openFile(videoFrame* frame) {

    if (frame == nullptr) {
        return false;
    }

    AVDictionary* dict = nullptr;
    AVDictionary* opt  = nullptr;
    char value[24]     = {0};
    struct tm* lt;
    time_t curtime;

    filename_ = generateFileName();
    MA_LOGI(TAG, "save to %s", filename_.c_str());

    avFmtCtx_ = avformat_alloc_context();

    int ret = avformat_alloc_output_context2(&avFmtCtx_, nullptr, nullptr, filename_.c_str());
    if (ret < 0) {
        MA_LOGW(TAG, "could not create output context: %d", ret);
        goto err;
    }

    avStream_ = avformat_new_stream(avFmtCtx_, nullptr);

    if (avStream_ == nullptr) {
        MA_LOGE(TAG, "could not create new stream");
        goto err;
    }

    avStream_->id                 = avFmtCtx_->nb_streams - 1;
    avStream_->time_base          = av_d2q(1.0 / frame->fps, INT_MAX);
    avStream_->codecpar->codec_id = AV_CODEC_ID_H264;
    avStream_->codecpar->channels = avStream_->codecpar->width = frame->img.width;
    avStream_->codecpar->height                                = frame->img.height;
    avStream_->codecpar->codec_type                            = AVMEDIA_TYPE_VIDEO;
    avStream_->codecpar->format                                = 0;

    audioStream_ = avformat_new_stream(avFmtCtx_, nullptr);
    if (audioStream_ == nullptr) {
        MA_LOGE(TAG, "could not create new stream");
        goto err;
    }
    audioStream_->id                       = avFmtCtx_->nb_streams - 1;
    audioStream_->time_base.num            = 1;
    audioStream_->time_base.den            = SAMPLE_RATE;
    audioStream_->codecpar->format         = AV_SAMPLE_FMT_S16;
    audioStream_->codecpar->codec_id       = AV_CODEC_ID_PCM_S16LE;
    audioStream_->codecpar->codec_type     = AVMEDIA_TYPE_AUDIO;
    audioStream_->codecpar->frame_size     = 1600;
    audioStream_->codecpar->channels       = CHANNELS;
    audioStream_->codecpar->sample_rate    = SAMPLE_RATE;
    audioStream_->codecpar->channel_layout = av_get_default_channel_layout(audioStream_->codecpar->channels);


    /* Enable defragment funciton in ffmpeg */
    time(&curtime);

    lt = localtime(&curtime);
    memset(value, 0, sizeof(value));
    strftime(value, sizeof(value), "%Y-%m-%d %H:%M:%S", lt);

    av_dict_set(&dict, "truncate", "false", 0);
    av_dict_set(&dict, "fsync", "false", 0);
    av_dict_set(&avFmtCtx_->metadata, "creation_time", value, 0);
    if (avio_open2(&avFmtCtx_->pb, filename_.c_str(), AVIO_FLAG_WRITE, nullptr, &dict) < 0) {
        MA_LOGE(TAG, "could not open %s", filename_.c_str());
        goto err;
    }
    av_dict_free(&dict);

    if (avformat_write_header(avFmtCtx_, &opt) < 0) {
        MA_LOGE(TAG, "write header failed");
        goto err;
    }
    return true;

err:
    if (avFmtCtx_) {
        if (avFmtCtx_->pb) {
            avio_closep(&avFmtCtx_->pb);
        }
        avformat_free_context(avFmtCtx_);
        avFmtCtx_    = nullptr;
        avStream_    = nullptr;
        audioStream_ = nullptr;
        filename_    = "";
        av_dict_free(&opt);
    }
    return false;
}

void SaveNode::closeFile() {
    if (avFmtCtx_ == nullptr || avStream_ == nullptr || avFmtCtx_->pb == nullptr) {
        return;
    }
    av_write_trailer(avFmtCtx_);

    if (avFmtCtx_->pb) {
        avio_closep(&avFmtCtx_->pb);
    }
    avformat_free_context(avFmtCtx_);
    avFmtCtx_ = nullptr;
    avStream_ = nullptr;
    filename_ = "";
}
void SaveNode::threadEntry() {
    ma_tick_t start_  = 0;
    Frame* frame      = nullptr;
    videoFrame* video = nullptr;
    audioFrame* audio = nullptr;
    AVPacket packet   = {0};
    begin_            = Tick::current();

    server_->response(id_, json::object({{"type", MA_MSG_TYPE_RESP}, {"name", "enabled"}, {"code", MA_OK}, {"data", enabled_.load()}}));

    while (started_) {
        Thread::exitCritical();
        if (frame_.fetch(reinterpret_cast<void**>(&frame), Tick::fromSeconds(2))) {
            Thread::enterCritical();
            if (!enabled_) {
                frame->release();
                continue;
            }
            if (frame->chn == CHN_H264) {
                video = static_cast<videoFrame*>(frame);
                if (recycle(video->img.size) == false) {
                    video->release();
                    closeFile();
                    enabled_ = false;
                    server_->response(id_, json::object({{"type", MA_MSG_TYPE_RESP}, {"name", "save"}, {"code", MA_ENOMEM}, {"data", "No space left on device"}}));
                    continue;
                }

                if (video->img.key) {
                    if (filename_.empty() || (slice_ > 0 && Tick::current() - start_ > Tick::fromSeconds(slice_))) {
                        start_  = Tick::current();
                        vcount_ = 0;
                        acount_ = 0;
                        if (filename_.empty()) {
                            begin_ = Tick::current();
                            MA_LOGI(TAG, "start recording");
                            server_->response(id_, json::object({{"type", MA_MSG_TYPE_RESP}, {"name", "enabled"}, {"code", MA_OK}, {"data", enabled_.load()}}));
                        }
                        closeFile();
                        if (!openFile(video)) {
                            enabled_ = false;
                            video->release();
                            server_->response(id_, json::object({{"type", MA_MSG_TYPE_RESP}, {"name", "save"}, {"code", MA_ENOMEM}, {"data", "No space left on device"}}));
                            frame->release();
                            continue;
                        }
                    }
                }
                if (avFmtCtx_ != nullptr) {
                    av_init_packet(&packet);
                    packet.pts          = vcount_++;
                    packet.dts          = packet.pts;
                    packet.data         = video->img.data;
                    packet.size         = video->img.size;
                    packet.flags        = video->img.key ? AV_PKT_FLAG_KEY : 0;
                    packet.stream_index = avStream_->index;
                    AVRational r        = av_d2q(1.0 / video->fps, INT_MAX);
                    av_packet_rescale_ts(&packet, r, avStream_->time_base);
                    int ret = av_write_frame(avFmtCtx_, &packet);
                    if (ret != 0) {
                        MA_LOGW(TAG, "write video (%d: size %d) failed %d", ret, vcount_, video->img.size);
                        closeFile();
                        enabled_ = false;
                        server_->response(id_, json::object({{"type", MA_MSG_TYPE_RESP}, {"name", "save"}, {"code", MA_ENOMEM}, {"data", "No space left on device"}}));
                        frame->release();
                        continue;
                    }
                    if ((duration_ > 0 && Tick::current() - begin_ > Tick::fromSeconds(duration_)) || begin_ == 0) {
                        closeFile();
                        enabled_ = false;
                        MA_LOGI(TAG, "stop recording");
                        server_->response(id_, json::object({{"type", MA_MSG_TYPE_RESP}, {"name", "enabled"}, {"code", MA_OK}, {"data", enabled_.load()}}));
                        frame->release();
                        continue;
                    }
                }
            }
            if (frame->chn == CHN_AUDIO) {
                audio = static_cast<audioFrame*>(frame);
                if (avFmtCtx_ != nullptr) {
                    av_init_packet(&packet);
                    packet.pts          = acount_++;
                    packet.dts          = packet.pts;
                    packet.data         = audio->data;
                    packet.size         = audio->size;
                    packet.duration     = 0;
                    packet.stream_index = audioStream_->index;
                    AVRational r        = av_d2q(1.0 / 20, INT_MAX);
                    av_packet_rescale_ts(&packet, r, audioStream_->time_base);
                    int ret = av_write_frame(avFmtCtx_, &packet);
                    if (ret != 0) {
                        MA_LOGW(TAG, "write audio (%d: size %d) failed %d", ret, acount_, audio->size);
                        closeFile();
                        enabled_ = false;
                        server_->response(id_, json::object({{"type", MA_MSG_TYPE_RESP}, {"name", "save"}, {"code", MA_ENOMEM}, {"data", "No space left on device"}}));
                        frame->release();
                        continue;
                    }
                }
            }
            frame->release();
        }
    }
}

void SaveNode::threadEntryStub(void* obj) {
    reinterpret_cast<SaveNode*>(obj)->threadEntry();
}

ma_err_t SaveNode::onCreate(const json& config) {
    Guard guard(mutex_);
    ma_err_t err = MA_OK;

    MA_LOGD(TAG, "config: %s", config.dump().c_str());

    if (!config.contains("storage") || !config.contains("slice")) {
        MA_THROW(Exception(MA_EINVAL, "Invalid config"));
    }

    if (!config["storage"].is_string() || !config["slice"].is_number()) {
        MA_THROW(Exception(MA_EINVAL, "Invalid config"));
    }

    if (config.contains("duration") && config["duration"].is_number()) {
        duration_ = config["duration"].get<int>();
    }

    if (config.contains("enabled") && config["enabled"].is_boolean()) {
        enabled_ = config["enabled"].get<bool>();
    }

    std::string storageType = config["storage"].get<std::string>();
    storage_                = (storageType == "local") ? NODE_SAVE_PATH_LOCAL : (storageType == "external") ? NODE_SAVE_PATH_EXTERNAL : "";


    if (storage_.empty()) {
        MA_THROW(Exception(MA_EINVAL, "Invalid storage type"));
    }

    if (!std::filesystem::exists(storage_) || !std::filesystem::is_directory(storage_)) {
        if (!std::filesystem::create_directory(storage_)) {
            MA_THROW(Exception(MA_EINVAL, "Failed to create storage directory"));
        }
    }

    slice_ = config["slice"].get<int>();

    thread_ = new Thread((type_ + "#" + id_).c_str(), threadEntryStub);
    if (thread_ == nullptr) {
        MA_THROW(Exception(MA_ENOMEM, "Not enough memory"));
    }

    std::filesystem::space_info si = std::filesystem::space(storage_);

    int64_t available = (si.available - NODE_MIN_AVILABLE_CAPACITY) / 1024;

    if (available < 0) {
        available = 0;
    }

    MA_LOGI(TAG, "storage: %s, slice: %d duration: %d available: %ldKB", storage_.c_str(), slice_, duration_, available);

    server_->response(id_,
                      json::object({{"type", MA_MSG_TYPE_RESP}, {"name", "create"}, {"code", err}, {"data", {"storage", storage_, "slice", slice_, "duration", duration_, "available", available}}}));

    created_ = true;

    return err;
}

ma_err_t SaveNode::onControl(const std::string& control, const json& data) {
    Guard guard(mutex_);
    if (control == "enabled" && data.is_boolean()) {
        bool enabled = data.get<bool>();
        if (enabled_.load() != enabled) {
            if (!enabled) {
                begin_ = 0;
            } else {
                enabled_.store(enabled);
            }
        }
        server_->response(id_, json::object({{"type", MA_MSG_TYPE_RESP}, {"name", control}, {"code", MA_OK}, {"data", enabled_.load()}}));
    } else {
        server_->response(id_, json::object({{"type", MA_MSG_TYPE_RESP}, {"name", control}, {"code", MA_ENOTSUP}, {"data", "Not supported"}}));
    }
    return MA_OK;
}


ma_err_t SaveNode::onDestroy() {
    Guard guard(mutex_);

    if (!created_) {
        return MA_OK;
    }

    onStop();

    if (thread_ != nullptr) {
        delete thread_;
        thread_ = nullptr;
    }

    created_ = false;

    return MA_OK;
}

ma_err_t SaveNode::onStart() {
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

    recycle();

    started_ = true;

    thread_->start(this);

    return MA_OK;
}

ma_err_t SaveNode::onStop() {
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


    closeFile();

    return MA_OK;
}


REGISTER_NODE("save", SaveNode);

}  // namespace ma::node
