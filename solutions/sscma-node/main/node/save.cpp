#include <filesystem>
#include <sys/statvfs.h>  // 用于 statvfs 函数

#include "save.h"

#ifndef NODE_SAVE_PATH_LOCAL
#define NODE_SAVE_PATH_LOCAL "/userdata/local/"
#endif

#ifndef NODE_SAVE_PATH_EXTERNAL
#define NODE_SAVE_PATH_EXTERNAL "/mnt/sd/"
#endif

#ifndef NODE_SAVE_MAX_SIZE
#define NODE_SAVE_MAX_SIZE 128 * 1024 * 1024
#endif

namespace ma::node {

static constexpr char TAG[] = "ma::node::save";

SaveNode::SaveNode(std::string id)
    : Node("save", id),
      storage_(NODE_SAVE_PATH_LOCAL),
      max_size_(NODE_SAVE_MAX_SIZE),
      slice_(300),
      camera_(nullptr),
      frame_(30),
      thread_(nullptr) {}

SaveNode::~SaveNode() {
    if (thread_ != nullptr) {
        delete thread_;
    }
    if (file_.is_open()) {
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
        std::sort(files.begin(), files.end(), [](const auto& a, const auto& b) {
            return std::filesystem::last_write_time(a) < std::filesystem::last_write_time(b);
        });
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
    while (true) {
        for (auto dep : dependencies_) {
            auto n = NodeFactory::find(dep);
            if (n == nullptr) {
                break;
            }
            if (camera_ == nullptr && n->type() == "camera") {
                Thread::enterCritical();
                camera_ = reinterpret_cast<CameraNode*>(n);
                camera_->config(CHN_H264);
                camera_->attach(CHN_H264, &frame_);
                camera_->onStart();
                Thread::exitCritical();
                break;
            }
        }
        if (camera_ != nullptr) {
            break;
        }
        Thread::sleep(Tick::fromMilliseconds(10));
    }

    if (slice_ <= 0) {
        file_.open(generateFileName(), std::ios::binary | std::ios::out);
        recycle();
    }

    while (true) {
        if (frame_.fetch(reinterpret_cast<void**>(&frame))) {
            Thread::enterCritical();
            if (slice_ > 0 && frame->timestamp >= start_ + Tick::fromSeconds(slice_)) {
                start_ = frame->timestamp;
                file_.flush();
                file_.close();
                file_.open(generateFileName(), std::ios::binary | std::ios::out);
                recycle();
                if (frame->count < 2) {
                    frame->release();
                    continue;
                }
            }
            file_.write(reinterpret_cast<char*>(frame->img.data), frame->img.size);
            frame->release();
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

    if (!config.contains("storage") || !config.contains("slice")) {
        throw NodeException(MA_EINVAL, "invalid config");
    }


    std::string storageType = config["storage"].get<std::string>();
    storage_                = (storageType == "local") ? NODE_SAVE_PATH_LOCAL
                       : (storageType == "external")   ? NODE_SAVE_PATH_EXTERNAL
                                                       : "";


    if (storage_.empty()) {
        throw NodeException(MA_EINVAL, "invalid storage type");
    }

    if (!std::filesystem::exists(storage_) || !std::filesystem::is_directory(storage_)) {
        throw NodeException(MA_EINVAL, "invalid storage path");
    }

    slice_ = config["slice"].get<int>();

    thread_ = new Thread((type_ + "#" + id_).c_str(), threadEntryStub);
    if (thread_ == nullptr) {
        throw NodeException(MA_ENOMEM, "thread create failed");
    }

    std::filesystem::space_info si = std::filesystem::space(storage_);

    if (storageType == "local") {
        max_size_ = si.available / 10 * 6;
    } else {
        max_size_ = si.available / 10 * 8;
    }


    MA_LOGI(TAG, "storage: %s, slice: %d max_size: %ldB", storage_.c_str(), slice_, max_size_);

    server_->response(
        id_,
        json::object(
            {{"type", MA_MSG_TYPE_RESP}, {"name", "create"}, {"code", err}, {"data", "23"}}));

    return err;
}

ma_err_t SaveNode::onMessage(const json& message) {
    Guard guard(mutex_);
    server_->response(id_,
                      json::object({{"type", MA_MSG_TYPE_RESP},
                                    {"name", message["name"]},
                                    {"code", MA_ENOTSUP},
                                    {"data", ""}}));
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

    server_->response(
        id_,
        json::object(
            {{"type", MA_MSG_TYPE_RESP}, {"name", "destroy"}, {"code", err}, {"data", ""}}));

    return MA_OK;
}

ma_err_t SaveNode::onStart() {
    Guard guard(mutex_);
    if (started_) {
        return MA_OK;
    }
    thread_->start(this);
    started_ = true;
    return MA_OK;
}

ma_err_t SaveNode::onStop() {
    Guard guard(mutex_);
    if (!started_) {
        return MA_OK;
    }
    if (camera_ != nullptr) {
        camera_->detach(CHN_H264, &frame_);
    }
    thread_->stop();
    started_ = false;
    return MA_OK;
}


REGISTER_NODE("save", SaveNode);

}  // namespace ma::node
