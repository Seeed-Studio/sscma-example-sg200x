// save.cpp
#include <filesystem>
#include <fstream>
#include <sys/statvfs.h>

#include "camera.h"  // Explicitly include camera.h to ensure audioFrame and videoFrame are available
#include "save.h"

// Default folders for saving
#ifndef NODE_SAVE_PATH_LOCAL
#define NODE_SAVE_PATH_LOCAL "/userdata/Videos/"
#endif

#ifndef NODE_SAVE_PATH_EXTERNAL
#define NODE_SAVE_PATH_EXTERNAL "/mnt/sd/Videos/"
#endif

#ifndef NODE_IMAGE_PATH_LOCAL
#define NODE_IMAGE_PATH_LOCAL "/userdata/Images/"
#endif

#ifndef NODE_IMAGE_PATH_EXTERNAL
#define NODE_IMAGE_PATH_EXTERNAL "/mnt/sd/Images/"
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
    : Node("save", id),
      storage_(NODE_SAVE_PATH_LOCAL),
      saveMode_("video"),
      slice_(300),
      duration_(-1),
      begin_(0),
      start_(0),
      manual_capture_requested_(false),
      vcount_(0),
      acount_(0),
      imageCount_(0),
      camera_(nullptr),
      frame_(60),
      thread_(nullptr),
      avFmtCtx_(nullptr),
      avStream_(nullptr),
      audioStream_(nullptr),
      audioCodec_(nullptr),
      audioCodecCtx_(nullptr),
      audio_buffer_() {}

SaveNode::~SaveNode() {
    onDestroy();
}

std::string SaveNode::generateFileName() {
    auto now = std::time(nullptr);
    std::ostringstream oss;
    oss << storage_ << std::put_time(std::localtime(&now), "%Y%m%d_%H%M%S") << ".mp4";
    return oss.str();
}

std::string SaveNode::generateImageFileName() {
    auto now = std::time(nullptr);
    std::ostringstream oss;
    oss << storage_ << std::put_time(std::localtime(&now), "%Y%m%d_%H%M%S") << "_" << std::setfill('0') << std::setw(4) << imageCount_ << ".jpg";
    return oss.str();
}

bool SaveNode::saveImage(videoFrame* frame) {
    if (frame == nullptr || frame->img.data == nullptr) {
        return false;
    }

    filename_ = generateImageFileName();
    MA_LOGI(TAG, "save image to %s", filename_.c_str());

    if (recycle(frame->img.size / 2) == false) {
        MA_LOGW(TAG, "No space left on device");
        return false;
    }

    std::ofstream file(filename_, std::ios::binary);
    if (!file.is_open()) {
        MA_LOGE(TAG, "could not open %s for writing", filename_.c_str());
        return false;
    }

    file.write(reinterpret_cast<const char*>(frame->img.data), frame->img.size);
    file.close();

    if (!file.good()) {
        MA_LOGE(TAG, "failed to write image data to %s", filename_.c_str());
        return false;
    }

    imageCount_++;
    MA_LOGI(TAG, "image saved successfully: %s (size: %d bytes)", filename_.c_str(), frame->img.size);
    return true;
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
    int ret   = avformat_alloc_output_context2(&avFmtCtx_, nullptr, "mp4", filename_.c_str());
    if (ret < 0) {
        MA_LOGW(TAG, "could not create output context: %d", ret);
        goto err;
    }

    avStream_ = avformat_new_stream(avFmtCtx_, nullptr);
    if (avStream_ == nullptr) {
        MA_LOGE(TAG, "could not create new stream");
        goto err;
    }

    avStream_->id                   = avFmtCtx_->nb_streams - 1;
    avStream_->time_base            = av_d2q(1.0 / frame->fps, INT_MAX);
    avStream_->codecpar->codec_id   = AV_CODEC_ID_H264;
    avStream_->codecpar->width      = frame->img.width;
    avStream_->codecpar->height     = frame->img.height;
    avStream_->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
    avStream_->codecpar->format     = 0;

    audioStream_ = avformat_new_stream(avFmtCtx_, nullptr);
    if (audioStream_ == nullptr) {
        MA_LOGE(TAG, "could not create new stream");
        goto err;
    }

    audioCodec_ = avcodec_find_encoder(AV_CODEC_ID_AAC);
    if (!audioCodec_) {
        MA_LOGE(TAG, "could not find AAC encoder");
        goto err;
    }

    audioCodecCtx_ = avcodec_alloc_context3(audioCodec_);
    if (!audioCodecCtx_) {
        MA_LOGE(TAG, "could not allocate AAC codec context");
        goto err;
    }

    audioCodecCtx_->bit_rate       = 128000;
    audioCodecCtx_->sample_rate    = SAMPLE_RATE;
    audioCodecCtx_->channels       = CHANNELS;
    audioCodecCtx_->channel_layout = av_get_default_channel_layout(CHANNELS);
    audioCodecCtx_->sample_fmt     = AV_SAMPLE_FMT_FLTP;
    audioCodecCtx_->time_base      = {1, SAMPLE_RATE};

    ret = avcodec_open2(audioCodecCtx_, audioCodec_, nullptr);
    if (ret < 0) {
        MA_LOGE(TAG, "could not open AAC codec: %d", ret);
        goto err;
    }

    audioStream_->id                       = avFmtCtx_->nb_streams - 1;
    audioStream_->time_base                = {1, SAMPLE_RATE};
    audioStream_->codecpar->codec_id       = AV_CODEC_ID_AAC;
    audioStream_->codecpar->codec_type     = AVMEDIA_TYPE_AUDIO;
    audioStream_->codecpar->sample_rate    = SAMPLE_RATE;
    audioStream_->codecpar->channels       = CHANNELS;
    audioStream_->codecpar->channel_layout = av_get_default_channel_layout(CHANNELS);
    audioStream_->codecpar->format         = AV_SAMPLE_FMT_FLTP;

    ret = avcodec_parameters_from_context(audioStream_->codecpar, audioCodecCtx_);
    if (ret < 0) {
        MA_LOGE(TAG, "could not copy codec parameters: %d", ret);
        goto err;
    }

    time(&curtime);
    lt = localtime(&curtime);
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
        if (audioCodecCtx_) {
            avcodec_free_context(&audioCodecCtx_);
            audioCodecCtx_ = nullptr;
        }
        audioCodec_ = nullptr;
        filename_   = "";
        av_dict_free(&opt);
    }
    return false;
}

void SaveNode::closeFile() {
    if (avFmtCtx_ == nullptr || avStream_ == nullptr || avFmtCtx_->pb == nullptr) {
        return;
    }

    if (avFmtCtx_ && audioCodecCtx_) {
        // Flush remaining audio data by processing buffered PCM
        size_t sample_bytes = CHANNELS * 2;  // Bytes per sample (S16LE)
        size_t needed_bytes = audioCodecCtx_->frame_size * sample_bytes;
        while (audio_buffer_.size() >= needed_bytes) {
            AVFrame* audioFrame        = av_frame_alloc();
            audioFrame->nb_samples     = audioCodecCtx_->frame_size;
            audioFrame->format         = AV_SAMPLE_FMT_FLTP;
            audioFrame->channel_layout = audioCodecCtx_->channel_layout;
            audioFrame->sample_rate    = audioCodecCtx_->sample_rate;

            int ret = av_frame_get_buffer(audioFrame, 0);
            if (ret < 0) {
                MA_LOGW(TAG, "could not allocate audio frame buffer: %d", ret);
                av_frame_free(&audioFrame);
                break;
            }

            const int16_t* pcm_data = (const int16_t*)audio_buffer_.data();
            int samples             = audioCodecCtx_->frame_size;
            for (int i = 0; i < samples; i++) {
                for (int ch = 0; ch < CHANNELS; ch++) {
                    float val                         = pcm_data[i * CHANNELS + ch] / 32768.0f;
                    ((float*)audioFrame->data[ch])[i] = val;
                }
            }

            audio_buffer_.erase(audio_buffer_.begin(), audio_buffer_.begin() + needed_bytes);

            audioFrame->pts = acount_ * audioCodecCtx_->frame_size;
            acount_++;

            ret = avcodec_send_frame(audioCodecCtx_, audioFrame);
            av_frame_free(&audioFrame);
            if (ret < 0) {
                MA_LOGW(TAG, "error sending audio frame to encoder: %d", ret);
                break;
            }

            while (true) {
                AVPacket pkt = {0};
                ret          = avcodec_receive_packet(audioCodecCtx_, &pkt);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                    break;
                if (ret < 0) {
                    MA_LOGW(TAG, "error receiving audio packet: %d", ret);
                    break;
                }

                pkt.stream_index = audioStream_->index;
                av_packet_rescale_ts(&pkt, audioCodecCtx_->time_base, audioStream_->time_base);
                ret = av_write_frame(avFmtCtx_, &pkt);
                av_packet_unref(&pkt);
                if (ret != 0) {
                    MA_LOGW(TAG, "write audio failed %d", ret);
                    break;
                }
            }
        }

        if (!audio_buffer_.empty()) {
            // Pad the remaining buffer to full frame size with silence
            audio_buffer_.resize(needed_bytes, 0);

            AVFrame* audioFrame        = av_frame_alloc();
            audioFrame->nb_samples     = audioCodecCtx_->frame_size;
            audioFrame->format         = AV_SAMPLE_FMT_FLTP;
            audioFrame->channel_layout = audioCodecCtx_->channel_layout;
            audioFrame->sample_rate    = audioCodecCtx_->sample_rate;

            int ret = av_frame_get_buffer(audioFrame, 0);
            if (ret < 0) {
                MA_LOGW(TAG, "could not allocate audio frame buffer: %d", ret);
                av_frame_free(&audioFrame);
            } else {
                const int16_t* pcm_data = (const int16_t*)audio_buffer_.data();
                int samples             = audioCodecCtx_->frame_size;
                for (int i = 0; i < samples; i++) {
                    for (int ch = 0; ch < CHANNELS; ch++) {
                        float val                         = pcm_data[i * CHANNELS + ch] / 32768.0f;
                        ((float*)audioFrame->data[ch])[i] = val;
                    }
                }

                audio_buffer_.clear();

                audioFrame->pts = acount_ * audioCodecCtx_->frame_size;
                acount_++;

                ret = avcodec_send_frame(audioCodecCtx_, audioFrame);
                av_frame_free(&audioFrame);
                if (ret < 0) {
                    MA_LOGW(TAG, "error sending padded audio frame to encoder: %d", ret);
                } else {
                    while (true) {
                        AVPacket pkt = {0};
                        ret          = avcodec_receive_packet(audioCodecCtx_, &pkt);
                        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                            break;
                        if (ret < 0) {
                            MA_LOGW(TAG, "error receiving padded audio packet: %d", ret);
                            break;
                        }

                        pkt.stream_index = audioStream_->index;
                        av_packet_rescale_ts(&pkt, audioCodecCtx_->time_base, audioStream_->time_base);
                        ret = av_write_frame(avFmtCtx_, &pkt);
                        av_packet_unref(&pkt);
                        if (ret != 0) {
                            MA_LOGW(TAG, "write padded audio failed %d", ret);
                            break;
                        }
                    }
                }
            }
        }

        // Flush the AAC encoder
        int ret = avcodec_send_frame(audioCodecCtx_, nullptr);
        if (ret < 0 && ret != AVERROR_EOF) {
            MA_LOGW(TAG, "error flushing audio encoder: %d", ret);
        } else {
            while (true) {
                AVPacket pkt = {0};
                ret          = avcodec_receive_packet(audioCodecCtx_, &pkt);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                    break;
                if (ret < 0) {
                    MA_LOGW(TAG, "error receiving flushed audio packet: %d", ret);
                    break;
                }

                pkt.stream_index = audioStream_->index;
                av_packet_rescale_ts(&pkt, audioCodecCtx_->time_base, audioStream_->time_base);
                ret = av_write_frame(avFmtCtx_, &pkt);
                av_packet_unref(&pkt);
                if (ret != 0) {
                    MA_LOGW(TAG, "write flushed audio failed %d", ret);
                    break;
                }
            }
        }
    }

    av_write_trailer(avFmtCtx_);

    if (avFmtCtx_->pb) {
        avio_closep(&avFmtCtx_->pb);
    }
    avformat_free_context(avFmtCtx_);
    if (audioCodecCtx_) {
        avcodec_free_context(&audioCodecCtx_);
        audioCodecCtx_ = nullptr;
    }
    avFmtCtx_    = nullptr;
    avStream_    = nullptr;
    audioStream_ = nullptr;
    audioCodec_  = nullptr;
    filename_    = "";
    audio_buffer_.clear();
}

void SaveNode::threadEntry() {
    ma_tick_t start_            = 0;
    Frame* frame                = nullptr;
    videoFrame* video           = nullptr;
    ma::node::audioFrame* audio = nullptr;  // Explicitly qualify audioFrame with namespace
    AVPacket packet             = {0};
    begin_                      = Tick::current();

    server_->response(id_, json::object({{"type", MA_MSG_TYPE_RESP}, {"name", "enabled"}, {"code", MA_OK}, {"data", enabled_.load()}}));

    while (started_) {
        Thread::exitCritical();

        if (frame_.fetch(reinterpret_cast<void**>(&frame), Tick::fromSeconds(2))) {
            Thread::enterCritical();
            if (!enabled_) {
                frame->release();
                continue;
            }
            if (saveMode_ == "image" && frame->chn == CHN_JPEG) {
                video = static_cast<videoFrame*>(frame);

                bool shouldSave = false;

                if (duration_ == 0) {
                    if (manual_capture_requested_) {
                        shouldSave                = true;
                        manual_capture_requested_ = false;
                    }
                } else {
                    if (slice_ > 0 && Tick::current() - start_ > Tick::fromSeconds(slice_)) {
                        shouldSave = true;
                        start_     = Tick::current();
                    }
                }

                if (shouldSave) {
                    if (saveImage(video)) {
                        MA_LOGI(TAG, "image saved %s", (duration_ == 0) ? "manually" : "at interval");
                        if (duration_ == 0) {
                            enabled_ = true;
                            server_->response(id_, json::object({{"type", MA_MSG_TYPE_RESP}, {"name", "enabled"}, {"code", MA_OK}, {"data", enabled_.load()}}));
                        }
                    } else {
                        MA_LOGW(TAG, "failed to save image");
                    }
                }

                if (duration_ > 0 && Tick::current() - begin_ > Tick::fromSeconds(duration_)) {
                    enabled_ = false;
                    MA_LOGI(TAG, "stop image saving - duration reached");
                    server_->response(id_, json::object({{"type", MA_MSG_TYPE_RESP}, {"name", "enabled"}, {"code", MA_OK}, {"data", enabled_.load()}}));
                    frame->release();
                    continue;
                }

                frame->release();
                continue;
            } else if (saveMode_ == "video" && frame->chn == CHN_H264) {
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
            } else if (saveMode_ == "video" && frame->chn == CHN_AUDIO) {
                audio = static_cast<ma::node::audioFrame*>(frame);  // Explicitly qualify audioFrame
                if (avFmtCtx_ != nullptr && audioCodecCtx_ != nullptr) {

                    audio_buffer_.insert(audio_buffer_.end(), audio->data, audio->data + audio->size);

                    size_t sample_bytes = CHANNELS * 2;  // Bytes per sample (S16LE)
                    size_t needed_bytes = audioCodecCtx_->frame_size * sample_bytes;
                    while (audio_buffer_.size() >= needed_bytes) {
                        AVFrame* audioFrame = av_frame_alloc();
                        audioFrame->nb_samples     = audioCodecCtx_->frame_size;
                        audioFrame->format         = AV_SAMPLE_FMT_FLTP;
                        audioFrame->channel_layout = audioCodecCtx_->channel_layout;
                        audioFrame->sample_rate    = audioCodecCtx_->sample_rate;
                        int ret = av_frame_get_buffer(audioFrame, 0);
                        if (ret < 0) {
                            MA_LOGW(TAG, "could not allocate audio frame buffer: %d", ret);
                            av_frame_free(&audioFrame);
                            frame->release();
                            continue;
                        }

                        const int16_t* pcm_data = (const int16_t*)audio_buffer_.data();
                        int samples             = audioCodecCtx_->frame_size;
                        for (int i = 0; i < samples; i++) {
                            for (int ch = 0; ch < CHANNELS; ch++) {
                                float val                         = pcm_data[i * CHANNELS + ch] / 32768.0f;
                                ((float*)audioFrame->data[ch])[i] = val;
                            }
                        }

                        audio_buffer_.erase(audio_buffer_.begin(), audio_buffer_.begin() + needed_bytes);

                        audioFrame->pts = acount_ * audioCodecCtx_->frame_size;
                        acount_++;

                        ret = avcodec_send_frame(audioCodecCtx_, audioFrame);
                        av_frame_free(&audioFrame);
                        if (ret < 0) {
                            MA_LOGW(TAG, "error sending audio frame to encoder: %d", ret);
                            frame->release();
                            continue;
                        }

                        while (true) {
                            AVPacket pkt = {0};
                            ret          = avcodec_receive_packet(audioCodecCtx_, &pkt);
                            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                                break;
                            if (ret < 0) {
                                MA_LOGW(TAG, "error receiving audio packet: %d", ret);
                                break;
                            }

                            pkt.stream_index = audioStream_->index;
                            av_packet_rescale_ts(&pkt, audioCodecCtx_->time_base, audioStream_->time_base);
                            ret = av_write_frame(avFmtCtx_, &pkt);
                            av_packet_unref(&pkt);
                            if (ret != 0) {
                                MA_LOGW(TAG, "write audio (%d: size %d) failed %d", ret, acount_, audio->size);
                                closeFile();
                                enabled_ = false;
                                server_->response(id_, json::object({{"type", MA_MSG_TYPE_RESP}, {"name", "save"}, {"code", MA_ENOMEM}, {"data", "No space left on device"}}));
                                break;
                            }
                        }
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

    if (config.contains("saveMode") && config["saveMode"].is_string()) {
        saveMode_ = config["saveMode"].get<std::string>();
        if (saveMode_ != "video" && saveMode_ != "image") {
            saveMode_ = "video";
        }
    }

    if (config.contains("duration") && config["duration"].is_number()) {
        duration_ = config["duration"].get<int>();
    }

    if (config.contains("enabled") && config["enabled"].is_boolean()) {
        enabled_ = config["enabled"].get<bool>();
    }

    std::string storageType = config["storage"].get<std::string>();
    if (saveMode_ == "image") {
        storage_ = (storageType == "local") ? NODE_IMAGE_PATH_LOCAL : (storageType == "external") ? NODE_IMAGE_PATH_EXTERNAL : "";
    } else {
        storage_ = (storageType == "local") ? NODE_SAVE_PATH_LOCAL : (storageType == "external") ? NODE_SAVE_PATH_EXTERNAL : "";
    }

    if (storage_.empty()) {
        MA_THROW(Exception(MA_EINVAL, "Invalid storage type"));
    }

    if (!std::filesystem::exists(storage_) || !std::filesystem::is_directory(storage_)) {
        if (!std::filesystem::create_directories(storage_)) {
            MA_THROW(Exception(MA_EINVAL, "Failed to create storage directory"));
        }
    }

    slice_ = config["slice"].get<int>();

    thread_ = new Thread((type_ + "#" + id_).c_str(), threadEntryStub);
    if (thread_ == nullptr) {
        MA_THROW(Exception(MA_ENOMEM, "Not enough memory"));
    }

    std::filesystem::space_info si = std::filesystem::space(storage_);
    int64_t available              = (si.available - NODE_MIN_AVILABLE_CAPACITY) / 1024;
    if (available < 0) {
        available = 0;
    }

    MA_LOGI(TAG, "storage: %s, saveMode: %s, slice: %d duration: %d available: %ldKB", storage_.c_str(), saveMode_.c_str(), slice_, duration_, available);

    server_->response(
        id_,
        json::object(
            {{"type", MA_MSG_TYPE_RESP}, {"name", "create"}, {"code", err}, {"data", {"storage", storage_, "saveMode", saveMode_, "slice", slice_, "duration", duration_, "available", available}}}));

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
    } else if (control == "capture" && saveMode_ == "image") {
        MA_LOGI(TAG, "Received capture command, enabled: %d, saveMode: %s", enabled_.load(), saveMode_.c_str());
        if (enabled_.load()) {
            manual_capture_requested_ = true;
            MA_LOGI(TAG, "Capture triggered, manual_capture_requested_ set to true");
            server_->response(id_, json::object({{"type", MA_MSG_TYPE_RESP}, {"name", control}, {"code", MA_OK}, {"data", "Capture triggered"}}));
        } else {
            MA_LOGW(TAG, "Capture command rejected - save node not enabled");
            server_->response(id_, json::object({{"type", MA_MSG_TYPE_RESP}, {"name", control}, {"code", MA_EBUSY}, {"data", "Save node not enabled"}}));
        }
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

    if (saveMode_ == "image") {
        camera_->attach(CHN_JPEG, &frame_);
        MA_LOGI(TAG, "attached to JPEG channel for image saving (using model's configuration)");
    } else {
        camera_->config(CHN_H264);
        camera_->attach(CHN_H264, &frame_);
        camera_->attach(CHN_AUDIO, &frame_);
        MA_LOGI(TAG, "configured H264 and audio channels for video saving");
    }

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
        if (saveMode_ == "image") {
            camera_->detach(CHN_JPEG, &frame_);
        } else {
            camera_->detach(CHN_H264, &frame_);
            camera_->detach(CHN_AUDIO, &frame_);
        }
    }

    closeFile();
    return MA_OK;
}

REGISTER_NODE("save", SaveNode);

}  // namespace ma::node