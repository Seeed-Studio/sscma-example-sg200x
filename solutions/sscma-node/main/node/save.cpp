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

bool SaveNode::recycle(uint32_t req_size) {
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

    if (total_size + req_size > max_size_) {
        std::sort(files.begin(), files.end(), [](const auto& a, const auto& b) { return std::filesystem::last_write_time(a) < std::filesystem::last_write_time(b); });
        for (const auto& file : files) {
            if (file.path() == filename_) {
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

    return cur_size_ + req_size <= max_size_;
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
    avStream_ = avformat_new_stream(avFmtCtx_, nullptr);

    int ret = avformat_alloc_output_context2(&avFmtCtx_, nullptr, nullptr, filename_.c_str());
    if (ret < 0) {
        MA_LOGW(TAG, "avformat_alloc_output_context2 failed: %d", ret);
        goto err;
    }

    avStream_ = avformat_new_stream(avFmtCtx_, nullptr);

    if (avStream_ == nullptr) {
        MA_LOGE(TAG, "avformat_new_stream failed");
        goto err;
    }

    avStream_->id                 = avFmtCtx_->nb_streams - 1;
    avStream_->time_base          = av_d2q(1.0 / frame->fps, INT_MAX);
    avStream_->codecpar->codec_id = AV_CODEC_ID_H264;
    avStream_->codecpar->channels = avStream_->codecpar->width = frame->img.width;
    avStream_->codecpar->height                                = frame->img.height;
    avStream_->codecpar->codec_type                            = AVMEDIA_TYPE_VIDEO;
    avStream_->codecpar->format                                = 0;

    /* Enable defragment funciton in ffmpeg */
    time(&curtime);

    lt = localtime(&curtime);
    memset(value, 0, sizeof(value));
    strftime(value, sizeof(value), "%Y-%m-%d %H:%M:%S", lt);

    av_dict_set(&dict, "truncate", "false", 0);
    av_dict_set(&dict, "fsync", "false", 0);
    av_dict_set(&avFmtCtx_->metadata, "creation_time", value, 0);
    if (avio_open2(&avFmtCtx_->pb, filename_.c_str(), AVIO_FLAG_WRITE, nullptr, &dict) < 0) {
        MA_LOGE(TAG, "avio_open2 failed");
        goto err;
    }
    av_dict_free(&dict);

    if (avformat_write_header(avFmtCtx_, &opt) < 0) {
        MA_LOGE(TAG, "avformat_write_header failed");
        goto err;
    }
    return true;

err:
    if (avFmtCtx_) {
        if (avFmtCtx_->pb) {
            avio_closep(&avFmtCtx_->pb);
        }
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
    videoFrame* frame = nullptr;
    AVPacket packet   = {0};
    begin_            = Tick::current();
    while (started_) {
        Thread::exitCritical();
        if (frame_.fetch(reinterpret_cast<void**>(&frame), Tick::fromSeconds(2))) {
            Thread::enterCritical();
            if (!enabled_) {
                frame->release();
                continue;
            }
            if (recycle(frame->img.size) == false) {
                frame->release();
                closeFile();
                enabled_ = false;
                server_->response(id_, json::object({{"type", MA_MSG_TYPE_RESP}, {"name", "save"}, {"code", MA_ENOMEM}, {"data", "No space left on device"}}));
                continue;
            }

            if (frame->img.key) {
                if (filename_.empty() || (slice_ > 0 && Tick::current() - start_ > Tick::fromSeconds(slice_) + Tick::toMilliseconds(500))) {  // 500ms for video length
                    start_ = Tick::current();
                    count_ = 0;
                    if (filename_.empty()) {
                        begin_ = Tick::current();
                        MA_LOGI(TAG, "start recording");
                        server_->response(id_, json::object({{"type", MA_MSG_TYPE_RESP}, {"name", "start"}, {"code", MA_OK}, {"data", ""}}));
                    }
                    closeFile();
                    if (!openFile(frame)) {
                        enabled_ = false;
                        frame->release();
                        server_->response(id_, json::object({{"type", MA_MSG_TYPE_RESP}, {"name", "save"}, {"code", MA_ENOMEM}, {"data", "No space left on device"}}));
                        continue;
                    }
                }
            }
            if (avFmtCtx_ != nullptr) {
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
                if (duration_ > 0 && Tick::current() - begin_ > Tick::fromSeconds(duration_) + Tick::toMilliseconds(500)) {
                    closeFile();
                    enabled_ = false;
                    MA_LOGI(TAG, "stop recording");
                    server_->response(id_, json::object({{"type", MA_MSG_TYPE_RESP}, {"name", "stop"}, {"code", MA_OK}}));
                    break;
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
    }


    closeFile();

    return MA_OK;
}


REGISTER_NODE("save", SaveNode);

}  // namespace ma::node
