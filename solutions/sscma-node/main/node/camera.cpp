#include "camera.h"

namespace ma::node {

static constexpr char TAG[] = "ma::node::camera";

const char* VIDEO_FORMATS[] = {"raw", "jpeg", "h264"};

#define CAMERA_INIT()                               \
    {                                               \
        Thread::enterCritical();                    \
        Thread::sleep(Tick::fromMilliseconds(100)); \
        MA_LOGI(TAG, "start video");                \
        startVideo();                               \
        started_ = true;                            \
        Thread::sleep(Tick::fromSeconds(1));        \
        Thread::exitCritical();                     \
    }

#define CAMERA_DEINIT()                             \
    {                                               \
        Thread::enterCritical();                    \
        MA_LOGI(TAG, "stop video");                 \
        started_ = false;                           \
        Thread::sleep(Tick::fromMilliseconds(100)); \
        deinitVideo();                              \
        MA_LOGI(TAG, "stop video done");            \
        Thread::sleep(Tick::fromSeconds(1));        \
        Thread::exitCritical();                     \
    }

CameraNode::CameraNode(std::string id) : Node("camera", std::move(id)), channels_(CHN_MAX), option_(0) {
    for (int i = 0; i < CHN_MAX; i++) {
        channels_[i].configured = false;
        channels_[i].enabled    = false;
        channels_[i].format     = MA_PIXEL_FORMAT_H264;
    }
    channels_.shrink_to_fit();
}

CameraNode::~CameraNode() {
    onDestroy();
};

static inline bool isKeyFrame(int format) {
    bool isKey = false;
    switch (format) {
        case H264E_NALU_ISLICE:
        case H264E_NALU_SPS:
        case H264E_NALU_IDRSLICE:
        case H264E_NALU_SEI:
        case H264E_NALU_PPS:
            isKey = true;
            break;
    }
    return isKey;
}

int CameraNode::vencCallback(void* pData, void* pArgs) {

    APP_DATA_CTX_S* pstDataCtx        = (APP_DATA_CTX_S*)pArgs;
    APP_DATA_PARAM_S* pstDataParam    = &pstDataCtx->stDataParam;
    APP_VENC_CHN_CFG_S* pstVencChnCfg = (APP_VENC_CHN_CFG_S*)pstDataParam->pParam;
    VENC_CHN VencChn                  = pstVencChnCfg->VencChn;

    if (!started_ || channels_[VencChn].msgboxes.empty()) {
        return CVI_SUCCESS;
    }
    if (pstVencChnCfg->VencChn >= CHN_MAX) {
        MA_LOGW(TAG, "invalid chn %d", pstVencChnCfg->VencChn);
        return CVI_SUCCESS;
    }

    VENC_STREAM_S* pstStream = (VENC_STREAM_S*)pData;
    VENC_PACK_S* ppack;

    for (int i = 0; i < pstStream->u32PackCount; i++) {
        videoFrame* frame = nullptr;
        ppack             = &pstStream->pstPack[i];
        if (VencChn == CHN_H264 && isKeyFrame(ppack->DataType.enH264EType)) {
            int cnt    = 0;
            int offset = 0;
            int size   = 0;
            for (int j = i; j < pstStream->u32PackCount; j++) {
                size += pstStream->pstPack[j].u32Len - pstStream->pstPack[j].u32Offset;
                cnt++;
                if (!isKeyFrame(pstStream->pstPack[j].DataType.enH264EType)) {
                    break;
                }
            }

            if (cnt == 1) {
                continue;
            }
            if (cnt == 2) {
                i += 1;
                cnt = 1;
            }
            frame                      = new videoFrame();
            frame->timestamp           = Tick::current();
            frame->img.width           = channels_[VencChn].width;
            frame->img.height          = channels_[VencChn].height;
            frame->img.format          = channels_[VencChn].format;
            frame->img.size            = size;
            frame->img.key             = true;
            frame->img.physical        = false;
            frame->img.data            = new uint8_t[size];
            frame->fps                 = channels_[VencChn].fps;
            channels_[VencChn].dropped = false;
            for (int j = i; j < i + cnt; j++) {
                memcpy(frame->img.data + offset, pstStream->pstPack[j].pu8Addr + pstStream->pstPack[j].u32Offset, pstStream->pstPack[j].u32Len - pstStream->pstPack[j].u32Offset);
                offset += pstStream->pstPack[j].u32Len - pstStream->pstPack[j].u32Offset;
            }
            i += (cnt - 1);
        } else {
            if (VencChn == CHN_H264 && channels_[VencChn].dropped) {
                continue;
            }
            frame               = new videoFrame();
            frame->timestamp    = Tick::current();
            frame->img.width    = channels_[VencChn].width;
            frame->img.height   = channels_[VencChn].height;
            frame->img.format   = channels_[VencChn].format;
            frame->img.size     = ppack->u32Len - ppack->u32Offset;
            frame->img.key      = false;
            frame->img.physical = false;
            frame->img.data     = new uint8_t[ppack->u32Len - ppack->u32Offset];
            frame->fps          = channels_[VencChn].fps;
            memcpy(frame->img.data, ppack->pu8Addr + ppack->u32Offset, ppack->u32Len - ppack->u32Offset);
        }
        if (frame != nullptr) {
            frame->ref(channels_[VencChn].msgboxes.size());
            for (auto& msgbox : channels_[VencChn].msgboxes) {
                if (!msgbox->post(frame, Tick::fromMilliseconds(static_cast<int>(1000.0 / channels_[VencChn].fps)))) {
                    frame->release();
                    channels_[VencChn].dropped = true;
                }
            }
        }
    }

    return CVI_SUCCESS;
}

int CameraNode::vpssCallback(void* pData, void* pArgs) {

    APP_VENC_CHN_CFG_S* pstVencChnCfg = (APP_VENC_CHN_CFG_S*)pArgs;
    VIDEO_FRAME_INFO_S* VpssFrame     = (VIDEO_FRAME_INFO_S*)pData;
    VIDEO_FRAME_S* f                  = &VpssFrame->stVFrame;

    if (!started_ || channels_[pstVencChnCfg->VencChn].msgboxes.empty()) {
        return CVI_SUCCESS;
    }
    if (pstVencChnCfg->VencChn >= CHN_MAX) {
        MA_LOGW(TAG, "invalid chn %d", pstVencChnCfg->VencChn);
        return CVI_SUCCESS;
    }

    videoFrame* frame   = new videoFrame();
    frame->img.size     = f->u32Length[0] + f->u32Length[1] + f->u32Length[2];
    frame->img.width    = channels_[pstVencChnCfg->VencChn].width;
    frame->img.height   = channels_[pstVencChnCfg->VencChn].height;
    frame->img.format   = channels_[pstVencChnCfg->VencChn].format;
    frame->img.key      = true;
    frame->img.physical = true;
    frame->img.data     = reinterpret_cast<uint8_t*>(f->u64PhyAddr[0]);
    frame->timestamp    = Tick::current();
    frame->fps          = channels_[pstVencChnCfg->VencChn].fps;
    for (auto& msgbox : channels_[pstVencChnCfg->VencChn].msgboxes) {
        if (!msgbox->post(frame, Tick::fromMilliseconds(5))) {
            frame->release();
        }
    }
    return CVI_SUCCESS;
}

int CameraNode::vencCallbackStub(void* pData, void* pArgs, void* pUserData) {
    return reinterpret_cast<CameraNode*>(pUserData)->vencCallback(pData, pArgs);
}

int CameraNode::vpssCallbackStub(void* pData, void* pArgs, void* pUserData) {
    APP_VENC_CHN_CFG_S* pstVencChnCfg = (APP_VENC_CHN_CFG_S*)pArgs;
    return reinterpret_cast<CameraNode*>(pUserData)->vpssCallback(pData, pArgs);
}

ma_err_t CameraNode::onCreate(const json& config) {
    Guard guard(mutex_);

    initVideo();

    option_ = 0;

    if (config.contains("option") && config["option"].is_string()) {
        std::string option = config["option"].get<std::string>();
        if (option.find("1080p") != std::string::npos) {
            option_ = 0;
        } else if (option.find("720p") != std::string::npos) {
            option_ = 1;
        } else if (option.find("480p") != std::string::npos) {
            option_ = 2;
        }
    }

    if (config.contains("option") && config["option"].is_number()) {
        int option = config["option"].get<int>();
    }

    switch (option_) {
        case 1:
            channels_[CHN_H264].format = MA_PIXEL_FORMAT_H264;
            channels_[CHN_H264].width  = 1280;
            channels_[CHN_H264].height = 1080;
            channels_[CHN_H264].fps    = 30;
            break;
        case 2:
            channels_[CHN_H264].format = MA_PIXEL_FORMAT_H264;
            channels_[CHN_H264].width  = 640;
            channels_[CHN_H264].height = 480;
            channels_[CHN_H264].fps    = 30;
            break;
        default:
            channels_[CHN_H264].format = MA_PIXEL_FORMAT_H264;
            channels_[CHN_H264].width  = 1920;
            channels_[CHN_H264].height = 1080;
            channels_[CHN_H264].fps    = 30;
            break;
    }

    server_->response(
        id_,
        json::object(
            {{"type", MA_MSG_TYPE_RESP}, {"name", "create"}, {"code", MA_OK}, {"data", {"width", channels_[CHN_H264].width, "height", channels_[CHN_H264].height, "fps", channels_[CHN_H264].fps}}}));

    created_ = true;

    return MA_OK;
}

ma_err_t CameraNode::onControl(const std::string& control, const json& data) {
    Guard guard(mutex_);
    server_->response(id_, json::object({{"type", MA_MSG_TYPE_RESP}, {"name", control}, {"code", MA_ENOTSUP}, {"data", ""}}));
    return MA_OK;
}

ma_err_t CameraNode::onDestroy() {
    Guard guard(mutex_);

    if (!created_) {
        return MA_OK;
    }

    onStop();

    created_ = false;

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

ma_err_t CameraNode::config(int chn, int32_t width, int32_t height, int32_t fps, ma_pixel_format_t format, bool enabled) {
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

    MA_LOGI(TAG, "config channel %d width %d height %d fps %d format %d", chn, channels_[chn].width, channels_[chn].height, channels_[chn].fps, channels_[chn].format);

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
            registerVideoFrameHandler(static_cast<video_ch_index_t>(chn), 0, vpssCallbackStub, this);
        } else {
            registerVideoFrameHandler(static_cast<video_ch_index_t>(chn), 0, vencCallbackStub, this);
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
        Thread::sleep(Tick::fromMilliseconds(2000 / channels_[chn].fps));  // skip 3frames
        channels_[chn].msgboxes.erase(it);
        started_ = true;
    }
    return MA_OK;
}

REGISTER_NODE_SINGLETON("camera", CameraNode);


}  // namespace ma::node