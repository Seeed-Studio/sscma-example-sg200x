#pragma once

extern "C" {
#include <libavcodec/codec_id.h>
#include <libavformat/avformat.h>
#include <libavutil/avassert.h>
#include <libavutil/log.h>
#include <libavutil/opt.h>
}

#include "camera.h"
#include "node.h"

#include "executor.hpp"

namespace ma::node {

class SaveNode : public Node {


public:
    SaveNode(std::string id);
    ~SaveNode();

    ma_err_t onCreate(const json& config) override;
    ma_err_t onStart() override;
    ma_err_t onControl(const std::string& control, const json& data) override;
    ma_err_t onStop() override;
    ma_err_t onDestroy() override;


protected:
    void threadEntry();
    static void threadEntryStub(void* obj);

private:
    std::string generateFileName();
    bool recycle(uint32_t req_size = 0);
    bool openFile(videoFrame* frame);
    void closeFile();

protected:
    std::string storage_;
    int slice_;
    int duration_;
    ma_tick_t begin_;
    int vcount_;
    int acount_;
    CameraNode* camera_;
    MessageBox frame_;
    Thread* thread_;
    std::string filename_;
    AVFormatContext* avFmtCtx_;
    AVStream* avStream_;
    AVStream* audioStream_;
};

}  // namespace ma::node