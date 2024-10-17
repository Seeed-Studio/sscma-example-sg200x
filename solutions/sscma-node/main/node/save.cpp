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

namespace ma::node {

static constexpr char TAG[] = "ma::node::save";

SaveNode::SaveNode(std::string id)
    : Node("save", id),
      storage_(NODE_SAVE_PATH_LOCAL),
      max_size_(NODE_SAVE_MAX_SIZE),
      enabled_(true),
      slice_(300),
      duration_(-1),
      count_(0),
      camera_(nullptr),
      frame_(30),
      thread_(nullptr),
      avFmtCtx_(nullptr),
      avStream_(nullptr) {
    av_log_set_level(AV_LOG_LEVEL);
}

SaveNode::~SaveNode() {
    onDestroy();
}

std::string SaveNode::generateFileName() {
    auto now = std::time(nullptr);
    std::ostringstream oss;
    oss << storage_ << std::put_time(std::localtime(&now), "%Y%m%d_%H%M%S") << ".mp4";
    return oss.str();
}

bool SaveNode::recycle(std::string filename, uint64_t size) {
    uint64_t total_size = 0;
    std::vector<std::filesystem::directory_entry> files;

    std::filesystem::space_info si = std::filesystem::space(storage_);

    max_size_ = si.available * 0.8;

    for (auto& p : std::filesystem::directory_iterator(storage_)) {
        if (p.is_regular_file()) {
            files.push_back(p);
            total_size += p.file_size();
        }
    }

    if (total_size > max_size_) {
        std::sort(files.begin(), files.end(), [](const auto& a, const auto& b) { return std::filesystem::last_write_time(a) < std::filesystem::last_write_time(b); });
        for (const auto& file : files) {
            if (file.path() == filename) {
                // skip the current file
                continue;
            }
            uint64_t file_size = file.file_size();
            if (std::filesystem::remove(file)) {
                MA_LOGI(TAG, "exceed max size %lu Bytes, remove %s", max_size_, file.path().c_str());
                total_size -= file_size;
                if (total_size <= max_size_) {
                    break;
                }
            }
        }
    }
    cur_size_ = total_size;
    return cur_size_ + size <= max_size_;
}

void SaveNode::threadEntry() {
    ma_tick_t start_  = 0;
    videoFrame* frame = nullptr;
    AVPacket packet   = {0};
    begin_            = Tick::current();

    while (true) {
        Thread::exitCritical();
        if (frame_.fetch(reinterpret_cast<void**>(&frame))) {
            Thread::enterCritical();

            if (enabled_ == false) {
                frame->release();
                continue;
            }

            if (cur_size_ + frame->img.size > max_size_) {
                if (recycle(filename_, frame->img.size) == false) {
                    frame->release();
                    MA_LOGE(TAG, "not enough space, stop recording");
                    if (avFmtCtx_ != nullptr && avFmtCtx_->pb != nullptr) {
                        av_write_trailer(avFmtCtx_);
                        avio_closep(&avFmtCtx_->pb);
                        avformat_free_context(avFmtCtx_);
                        avFmtCtx_ = nullptr;
                        avStream_ = nullptr;
                    }
                    enabled_ = false;
                    continue;
                }
            }

            // slice video
            if (frame->img.key && (avFmtCtx_ == nullptr || (slice_ > 0 && Tick::current() - start_ > Tick::fromSeconds(slice_)))) {
                if (avFmtCtx_ == nullptr) {
                    MA_LOGI(TAG, "start recording slice: %d duration: %d", slice_, duration_);
                    server_->response(id_, json::object({{"type", MA_MSG_TYPE_RESP}, {"name", "start"}, {"code", MA_OK}, {"data", ""}}));
                    begin_ = Tick::current();
                }
                filename_ = generateFileName();
                start_    = Tick::current();
                MA_LOGI(TAG, "save to %s", filename_.c_str());
                int ret = avformat_alloc_output_context2(&avFmtCtx_, nullptr, nullptr, filename_.c_str());
                if (ret < 0) {
                    MA_LOGW(TAG, "avformat_alloc_output_context2 failed: %d", ret);
                    frame->release();
                    continue;
                }

                avStream_ = avformat_new_stream(avFmtCtx_, nullptr);
                if (avStream_ == nullptr) {
                    MA_LOGE(TAG, "avformat_new_stream failed");
                    frame->release();
                    continue;
                }

                avStream_->id                 = avFmtCtx_->nb_streams - 1;
                avStream_->time_base          = av_d2q(1.0 / frame->fps, INT_MAX);
                avStream_->codecpar->codec_id = AV_CODEC_ID_H264;
                avStream_->codecpar->channels = avStream_->codecpar->width = frame->img.width;
                avStream_->codecpar->height                                = frame->img.height;
                avStream_->codecpar->codec_type                            = AVMEDIA_TYPE_VIDEO;
                avStream_->codecpar->format                                = 0;

                /* Enable defragment funciton in ffmpeg */
                count_         = 0;
                char value[24] = {0};
                struct tm* lt;
                time_t curtime;
                time(&curtime);

                lt = localtime(&curtime);
                memset(value, 0, sizeof(value));
                strftime(value, sizeof(value), "%Y-%m-%d %H:%M:%S", lt);

                AVDictionary* dict = NULL;
                av_dict_set(&dict, "truncate", "false", 0);
                av_dict_set(&dict, "fsync", "false", 0);
                av_dict_set(&avFmtCtx_->metadata, "creation_time", value, 0);
                if (avio_open2(&avFmtCtx_->pb, filename_.c_str(), AVIO_FLAG_WRITE, nullptr, &dict) < 0) {
                    MA_LOGE(TAG, "open filename_ failed");
                    av_dict_free(&dict);
                    avformat_free_context(avFmtCtx_);
                    avFmtCtx_ = nullptr;
                    avStream_ = nullptr;
                    frame->release();
                    continue;
                }
                av_dict_free(&dict);
                AVDictionary* opt = NULL;
                if (avformat_write_header(avFmtCtx_, &opt) < 0) {
                    MA_LOGE(TAG, "write header failed");
                    avio_closep(&avFmtCtx_->pb);
                    av_dict_free(&opt);
                    avformat_free_context(avFmtCtx_);
                    avFmtCtx_ = nullptr;
                    avStream_ = nullptr;
                    frame->release();
                    continue;
                }
            }
            if (avFmtCtx_ == nullptr) {
                frame->release();
                continue;
            }
            av_init_packet(&packet);
            packet.pts   = count_++;
            packet.dts   = packet.pts;
            packet.data  = frame->img.data;
            packet.size  = frame->img.size;
            packet.flags = frame->img.key ? AV_PKT_FLAG_KEY : 0;
            AVRational r = av_d2q(1.0 / frame->fps, INT_MAX);
            av_packet_rescale_ts(&packet, r, avStream_->time_base);
            if (av_write_frame(avFmtCtx_, &packet) != 0) {
                MA_LOGW(TAG, "write frame failed");
            }
            cur_size_ += frame->img.size;
            if (duration_ > 0 && frame->timestamp > begin_ + Tick::fromSeconds(duration_)) {
                MA_LOGI(TAG, "stop recording");
                server_->response(id_, json::object({{"type", MA_MSG_TYPE_RESP}, {"name", "stop"}, {"code", MA_OK}, {"data", ""}}));
                if (avFmtCtx_ != nullptr && avFmtCtx_->pb != nullptr) {
                    av_write_trailer(avFmtCtx_);
                    avio_closep(&avFmtCtx_->pb);
                    avformat_free_context(avFmtCtx_);
                    avFmtCtx_ = nullptr;
                    avStream_ = nullptr;
                }
                start_   = 0;
                enabled_ = false;
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
        MA_THROW(Exception(MA_EINVAL, "invalid config"));
    }

    if (!config["storage"].is_string() || !config["slice"].is_number()) {
        MA_THROW(Exception(MA_EINVAL, "invalid config"));
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
        MA_THROW(Exception(MA_EINVAL, "invalid storage type"));
    }

    if (!std::filesystem::exists(storage_) || !std::filesystem::is_directory(storage_)) {
        if (!std::filesystem::create_directory(storage_)) {
            MA_THROW(Exception(MA_EINVAL, "invalid storage path"));
        }
    }

    slice_ = config["slice"].get<int>();

    thread_ = new Thread((type_ + "#" + id_).c_str(), threadEntryStub);
    if (thread_ == nullptr) {
        MA_THROW(Exception(MA_ENOMEM, "no memory"));
    }

    std::filesystem::space_info si = std::filesystem::space(storage_);

    // max_size_ = 0.8 * space.total;
    max_size_ = si.available * 0.8;

    MA_LOGI(TAG, "storage: %s, slice: %d duration: %d max_size: %ldMB", storage_.c_str(), slice_, duration_, max_size_ / (1024 * 1024));

    server_->response(id_, json::object({{"type", MA_MSG_TYPE_RESP}, {"name", "create"}, {"code", err}, {"data", "23"}}));


    created_ = true;

    return err;
}

ma_err_t SaveNode::onControl(const std::string& control, const json& data) {
    Guard guard(mutex_);
    if (control == "enable") {
        if (!enabled_) {
            enabled_ = true;
        }
        server_->response(id_, json::object({{"type", MA_MSG_TYPE_RESP}, {"name", control}, {"code", MA_OK}, {"data", ""}}));
    } else if (control == "disable") {
        begin_ = 0;
        server_->response(id_, json::object({{"type", MA_MSG_TYPE_RESP}, {"name", control}, {"code", MA_OK}, {"data", ""}}));
    } else {
        server_->response(id_, json::object({{"type", MA_MSG_TYPE_RESP}, {"name", control}, {"code", MA_ENOTSUP}, {"data", ""}}));
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
        MA_THROW(Exception(MA_ENOTSUP, "camera not found"));
        return MA_ENOTSUP;
    }

    camera_->config(CHN_H264);
    camera_->attach(CHN_H264, &frame_);

    recycle("", 0);

    thread_->start(this);
    started_ = true;
    return MA_OK;
}

ma_err_t SaveNode::onStop() {
    Guard guard(mutex_);
    if (!started_) {
        return MA_OK;
    }
    started_ = false;

    if (camera_ != nullptr) {
        camera_->detach(CHN_H264, &frame_);
    }

    if (thread_ != nullptr) {
        thread_->stop();
    }

    if (avFmtCtx_ != nullptr) {
        av_write_trailer(avFmtCtx_);
        if (avFmtCtx_->pb != nullptr) {
            avio_closep(&avFmtCtx_->pb);
            avFmtCtx_->pb = nullptr;
        }
        avformat_free_context(avFmtCtx_);
        avFmtCtx_ = nullptr;
        avStream_ = nullptr;
    }
    return MA_OK;
}


REGISTER_NODE("save", SaveNode);

}  // namespace ma::node
