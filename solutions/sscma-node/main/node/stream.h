#pragma once

#include "camera.h"
#include "ma_transport_rtsp.h"
#include "node.h"

namespace ma::node {

class StreamNode : public Node {

public:
    StreamNode(std::string id);
    ~StreamNode();

    ma_err_t onCreate(const json& config) override;
    ma_err_t onStart() override;
    ma_err_t onMessage(const json& message) override;
    ma_err_t onStop() override;
    ma_err_t onDestroy() override;


protected:
    void threadEntry();
    static void threadEntryStub(void* obj);

protected:
    int port_;
    std::string session_;
    std::string host_;
    std::string url_;
    std::string username_;
    std::string password_;
    TransportRTSP* transport_;
    CameraNode* camera_;
    MessageBox frame_;
    Thread* thread_;
};

}  // namespace ma::node