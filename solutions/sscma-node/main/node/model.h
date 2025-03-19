
#pragma once

#include "extension/bytetrack/byte_tracker.h"
#include "extension/counter/counter.h"

#include "node.h"
#include "server.h"

#include "camera.h"

namespace ma::node {

class ModelNode : public Node {

public:
    ModelNode(std::string id);
    ~ModelNode();

    ma_err_t onCreate(const json& config) override;
    ma_err_t onStart() override;
    ma_err_t onControl(const std::string& control, const json& data) override;
    ma_err_t onStop() override;
    ma_err_t onDestroy() override;


protected:
    void threadEntry();
    static void threadEntryStub(void* obj);

protected:
    std::string uri_;
    int32_t times_;
    int32_t count_;
    bool debug_;
    bool trace_;
    bool counting_;
    json info_;
    int algorithm_;
    Model* model_;
    Engine* engine_;
    BYTETracker tracker_;
    Counter counter_;
    std::vector<std::string> labels_;
    Thread* thread_;
    CameraNode* camera_;
    MessageBox raw_frame_;
    MessageBox jpeg_frame_;
    bool websocket_;
    bool output_;
    TransportWebSocket* transport_;
};


}  // namespace ma::node