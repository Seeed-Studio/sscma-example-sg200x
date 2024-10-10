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

namespace ma::node {

static constexpr char TAG[] = "ma::node::save";

SaveNode::SaveNode(std::string id)
    : Node("save", id), storage_(NODE_SAVE_PATH_LOCAL), max_size_(NODE_SAVE_MAX_SIZE), enabled_(true), slice_(300), duration_(-1), camera_(nullptr), frame_(30), thread_(nullptr) {}

SaveNode::~SaveNode() {
    if (thread_ != nullptr) {
        delete thread_;
    }
    if (file_.is_open()) {
        file_.flush();
        file_.close();
    }
}

std::string SaveNode::generateFileName() {
    auto now = std::time(nullptr);
    std::ostringstream oss;
    oss << storage_ << std::put_time(std::localtime(&now), "%Y%m%d_%H%M%S") << ".h264";
    MA_LOGI(TAG, "save to %s", oss.str().c_str());
    return oss.str();
}

bool SaveNode::recycle() {
    uint64_t total_size = 0;
    std::vector<std::filesystem::directory_entry> files;

    for (auto& p : std::filesystem::directory_iterator(storage_)) {
        if (p.is_regular_file()) {
            files.push_back(p);
            total_size += p.file_size();
        }
    }

    if (total_size > max_size_) {
        std::sort(files.begin(), files.end(), [](const auto& a, const auto& b) { return std::filesystem::last_write_time(a) < std::filesystem::last_write_time(b); });
        for (const auto& file : files) {
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
    return false;
}

void SaveNode::threadEntry() {
    ma_tick_t start_  = 0;
    videoFrame* frame = nullptr;
    // wait for dependencies ready


    if (slice_ <= 0) {
        file_.open(generateFileName(), std::ios::binary | std::ios::out);
        recycle();
    }

    while (true) {
        if (frame_.fetch(reinterpret_cast<void**>(&frame))) {
            if (!enabled_) {
                frame->release();
                if (begin_ == 0) {
                    if (file_.is_open()) {
                        file_.flush();
                        file_.close();
                    }
                    begin_ = Tick::current();
                }
                continue;
            }
            Thread::enterCritical();
            if ((slice_ > 0 && frame->timestamp >= start_ + Tick::fromSeconds(slice_))) {
                start_ = frame->timestamp;
                file_.flush();
                file_.close();
                file_.open(generateFileName(), std::ios::binary | std::ios::out);
                recycle();
                if (frame->index != 0 || !frame->isKey) {
                    frame->release();
                    continue;
                }
            }
            file_.write(reinterpret_cast<char*>(frame->img.data), frame->img.size);
            frame->release();
            if (duration_ > 0 && frame->timestamp >= begin_ + Tick::fromSeconds(duration_)) {
                file_.flush();
                file_.close();
                start_   = 0;
                enabled_ = false;
            }
            Thread::exitCritical();
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

    if (storageType == "local") {
        max_size_ = si.available / 10 * 6;
    } else {
        max_size_ = si.available / 10 * 8;
    }


    MA_LOGI(TAG, "storage: %s, slice: %d duration: %d max_size: %ldB", storage_.c_str(), slice_, duration_, max_size_);

    server_->response(id_, json::object({{"type", MA_MSG_TYPE_RESP}, {"name", "create"}, {"code", err}, {"data", "23"}}));

    return err;
}

ma_err_t SaveNode::onControl(const std::string& control, const json& data) {
    Guard guard(mutex_);
    if (control == "enable") {
        if (!enabled_) {
            enabled_ = true;
        }
        begin_ = Tick::current();  // increase the duration
        server_->response(id_, json::object({{"type", MA_MSG_TYPE_RESP}, {"name", control}, {"code", MA_OK}, {"data", ""}}));
    } else if (control == "disable") {
        begin_   = 0;
        enabled_ = false;
        server_->response(id_, json::object({{"type", MA_MSG_TYPE_RESP}, {"name", control}, {"code", MA_OK}, {"data", ""}}));
    } else {
        server_->response(id_, json::object({{"type", MA_MSG_TYPE_RESP}, {"name", control}, {"code", MA_ENOTSUP}, {"data", ""}}));
    }
    return MA_OK;
}

ma_err_t SaveNode::onDestroy() {
    Guard guard(mutex_);
    ma_err_t err = MA_OK;

    if (thread_ != nullptr) {
        delete thread_;
        thread_ = nullptr;
    }

    if (file_.is_open()) {
        file_.close();
    }

    server_->response(id_, json::object({{"type", MA_MSG_TYPE_RESP}, {"name", "destroy"}, {"code", err}, {"data", ""}}));

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
    camera_->config(CHN_H264);
    camera_->attach(CHN_H264, &frame_);

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
    return MA_OK;
}


REGISTER_NODE("save", SaveNode);

}  // namespace ma::node
