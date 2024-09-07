#include "camera.h"

namespace ma::node {

static constexpr char TAG[] = "ma::node::camera";

const char* VIDEO_FORMATS[] = {"raw", "jpeg", "h264"};

#define CAMERA_INIT()                               \
    {                                               \
        Thread::enterCritical();                    \
        Thread::sleep(Tick::fromMilliseconds(100)); \
        MA_LOGD(TAG, "start video");                \
        startVideo();                               \
        started_ = true;                            \
        Thread::sleep(Tick::fromSeconds(1));        \
        Thread::exitCritical();                     \
    }

#define CAMERA_DEINIT()                             \
    {                                               \
        Thread::enterCritical();                    \
        MA_LOGD(TAG, "deinit video");               \
        started_ = false;                           \
        Thread::sleep(Tick::fromMilliseconds(100)); \
        deinitVideo();                              \
        Thread::sleep(Tick::fromSeconds(1));        \
        Thread::exitCritical();                     \
    }

CameraNode::CameraNode(std::string id)
    : Node("camera", std::move(id)), started_(false), channels_(CHN_MAX) {
    for (int i = 0; i < CHN_MAX; i++) {
        channels_[i].configured = false;
        channels_[i].enabled    = false;
        channels_[i].format     = MA_PIXEL_FORMAT_H264;
    }
}

CameraNode::~CameraNode() {
    if (started_) {
        CAMERA_DEINIT();
    }
};

int CameraNode::vencCallback(void* pData, void* pArgs) {

    APP_DATA_CTX_S* pstDataCtx        = (APP_DATA_CTX_S*)pArgs;
    APP_DATA_PARAM_S* pstDataParam    = &pstDataCtx->stDataParam;
    APP_VENC_CHN_CFG_S* pstVencChnCfg = (APP_VENC_CHN_CFG_S*)pstDataParam->pParam;
    VENC_CHN VencChn                  = pstVencChnCfg->VencChn;

    if (!started_) {
        return CVI_SUCCESS;
    }

    VENC_STREAM_S* pstStream = (VENC_STREAM_S*)pData;
    VENC_PACK_S* ppack;

    for (CVI_U32 i = 0; i < pstStream->u32PackCount; i++) {
        ppack             = &pstStream->pstPack[i];
        videoFrame* frame = new videoFrame();
        frame->timestamp  = Tick::current();
        frame->img.data   = new uint8_t[ppack->u32Len - ppack->u32Offset];
        memcpy(
            frame->img.data, ppack->pu8Addr + ppack->u32Offset, ppack->u32Len - ppack->u32Offset);
        frame->img.size   = ppack->u32Len - ppack->u32Offset;
        frame->img.width  = channels_[VencChn].width;
        frame->img.height = channels_[VencChn].height;
        frame->img.format = channels_[VencChn].format;
        frame->count      = pstStream->u32PackCount;
        frame->index      = i;
        frame->ref(channels_[VencChn].msgboxes.size());

        for (auto& msgbox : channels_[VencChn].msgboxes) {
            if (!msgbox->post(frame, Tick::fromMilliseconds(5))) {
                frame->release();
            }
        }
    }

    return CVI_SUCCESS;
}

int CameraNode::vpssCallback(void* pData, void* pArgs) {

    APP_VENC_CHN_CFG_S* pstVencChnCfg = (APP_VENC_CHN_CFG_S*)pArgs;
    VIDEO_FRAME_INFO_S* VpssFrame     = (VIDEO_FRAME_INFO_S*)pData;
    VIDEO_FRAME_S* f                  = &VpssFrame->stVFrame;

    if (!started_) {
        return CVI_SUCCESS;
    }

    videoFrame* frame = new videoFrame(false);
    frame->img.size   = f->u32Length[0] + f->u32Length[1] + f->u32Length[2];
    frame->img.width  = channels_[pstVencChnCfg->VencChn].width;
    frame->img.height = channels_[pstVencChnCfg->VencChn].height;
    frame->img.format = channels_[pstVencChnCfg->VencChn].format;
    frame->count      = 1;
    frame->index      = 1;
    frame->timestamp  = Tick::current();
    frame->phy_addr   = f->u64PhyAddr[0];
    for (auto& msgbox : channels_[pstVencChnCfg->VencChn].msgboxes) {
        if (!msgbox->post(frame, Tick::fromMilliseconds(30))) {
            frame->release();
        }
    }
    frame->wait();
    delete frame;

    return CVI_SUCCESS;
}

int CameraNode::vencCallbackStub(void* pData, void* pArgs, void* pUserData) {
    return reinterpret_cast<CameraNode*>(pUserData)->vencCallback(pData, pArgs);
}

int CameraNode::vpssCallbackStub(void* pData, void* pArgs, void* pUserData) {
    return reinterpret_cast<CameraNode*>(pUserData)->vpssCallback(pData, pArgs);
}

ma_err_t CameraNode::onCreate(const json& config) {
    Guard guard(mutex_);

    initVideo();

    if (config.is_array()) {
        for (auto& item : config) {
            if (!item.contains("channel")) {
                continue;
            }
            int channel = item["channel"].get<int>();
            if (channel < 0 || channel >= CHN_MAX) {
                throw NodeException(MA_EINVAL, "invalid channel");
            }
            if (item.contains("enabled") && item["enabled"].get<bool>()) {
                channels_[channel].width = item.contains("width") ? item["width"].get<int>() : 640;
                channels_[channel].height =
                    item.contains("height") ? item["height"].get<int>() : 640;
                channels_[channel].fps        = item.contains("fps") ? item["fps"].get<int>() : 30;
                channels_[channel].enabled    = true;
                channels_[channel].configured = false;
                MA_LOGI(TAG,
                        "create channel %d: %dx%d@%dfps %s enabled %d configured %d",
                        channel,
                        channels_[channel].width,
                        channels_[channel].height,
                        channels_[channel].fps,
                        VIDEO_FORMATS[channel],
                        channels_[channel].enabled,
                        channels_[channel].configured);
            }
        }
    }

    server_->response(
        id_,
        json::object(
            {{"type", MA_MSG_TYPE_RESP}, {"name", "create"}, {"code", MA_OK}, {"data", ""}}));

    return MA_OK;
}

ma_err_t CameraNode::onMessage(const json& message) {
    Guard guard(mutex_);
    server_->response(id_,
                      json::object({{"type", MA_MSG_TYPE_RESP},
                                    {"name", message["name"]},
                                    {"code", MA_ENOTSUP},
                                    {"data", ""}}));
    return MA_OK;
}

ma_err_t CameraNode::onDestroy() {
    Guard guard(mutex_);
    if (started_) {
        CAMERA_DEINIT();
    }
    server_->response(
        id_,
        json::object(
            {{"type", MA_MSG_TYPE_RESP}, {"name", "destroy"}, {"code", MA_OK}, {"data", ""}}));

    return MA_OK;
}

ma_err_t CameraNode::onStart() {
    Guard guard(mutex_);

    if (started_) {
        return MA_OK;
    }
    // checek enabled
    for (int i = 0; i < CHN_MAX; i++) {
        if (channels_[i].enabled) {
            break;
        }
        if (i == CHN_MAX - 1) {
            MA_LOGW(TAG, "no channel enabled");
            return MA_EINVAL;
        }
    }

    // check configured
    for (int i = 0; i < CHN_MAX; i++) {
        if (channels_[i].enabled && !channels_[i].configured) {
            return MA_AGAIN;
        }
    }

    CAMERA_INIT();
    return MA_OK;
}

ma_err_t CameraNode::onStop() {
    Guard guard(mutex_);
    if (!started_) {
        return MA_OK;
    }
    CAMERA_DEINIT();
    return MA_OK;
}

ma_err_t CameraNode::config(
    int chn, int32_t width, int32_t height, int32_t fps, ma_pixel_format_t format, bool enabled) {
    Guard guard(mutex_);
    if (chn < 0 || chn >= CHN_MAX) {
        return MA_EINVAL;
    }
    if (channels_[chn].configured) {
        return MA_EBUSY;
    }
    channels_[chn].width      = width > 0 ? width : channels_[chn].width;
    channels_[chn].height     = height > 0 ? height : channels_[chn].height;
    channels_[chn].fps        = fps > 0 ? fps : channels_[chn].fps;
    channels_[chn].format     = format != MA_PIXEL_FORMAT_UNKNOWN ? format : channels_[chn].format;
    channels_[chn].enabled    = enabled;
    channels_[chn].configured = true;

    video_ch_param_t param;

    MA_LOGD(TAG,
            "config channel %d width %d height %d fps %d format %d",
            chn,
            channels_[chn].width,
            channels_[chn].height,
            channels_[chn].fps,
            channels_[chn].format);

    switch (channels_[chn].format) {
        case MA_PIXEL_FORMAT_JPEG:
            param.format = VIDEO_FORMAT_JPEG;
            break;
        case MA_PIXEL_FORMAT_H264:
            param.format = VIDEO_FORMAT_H264;
            break;
        case MA_PIXEL_FORMAT_H265:
            param.format = VIDEO_FORMAT_H265;
            break;
        case MA_PIXEL_FORMAT_RGB888:
            param.format = VIDEO_FORMAT_RGB888;
            break;
        case MA_PIXEL_FORMAT_YUV422:
            param.format = VIDEO_FORMAT_NV21;
            break;
        default:
            break;
    }
    param.width  = channels_[chn].width;
    param.height = channels_[chn].height;
    param.fps    = channels_[chn].fps;
    if (channels_[chn].enabled) {
        setupVideo(static_cast<video_ch_index_t>(chn), &param);
        if (chn == CHN_RAW) {
            registerVideoFrameHandler(
                static_cast<video_ch_index_t>(chn), 0, vpssCallbackStub, this);
        } else {
            registerVideoFrameHandler(
                static_cast<video_ch_index_t>(chn), 0, vencCallbackStub, this);
        }
    }
    return MA_OK;
}

ma_err_t CameraNode::attach(int chn, MessageBox* msgbox) {
    Guard guard(mutex_);
    if (channels_[chn].enabled) {
        MA_LOGI(TAG, "attach %p to %d", msgbox, chn);
        channels_[chn].msgboxes.push_back(msgbox);
    }
    return MA_OK;
}

ma_err_t CameraNode::detach(int chn, MessageBox* msgbox) {
    Guard guard(mutex_);
    auto it = std::find(channels_[chn].msgboxes.begin(), channels_[chn].msgboxes.end(), msgbox);
    if (it != channels_[chn].msgboxes.end()) {
        MA_LOGI(TAG, "detach %p from %d", msgbox, chn);
        started_ = false;
        Thread::sleep(Tick::fromMilliseconds(100));
        channels_[chn].msgboxes.erase(it);
        started_ = true;
    }
    return MA_OK;
}

REGISTER_NODE_SINGLETON("camera", CameraNode);


}  // namespace ma::node