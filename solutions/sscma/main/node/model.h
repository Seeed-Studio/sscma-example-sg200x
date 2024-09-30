
#pragma once

#include "extension/bytetrack/byte_tracker.h"
#include "extension/counter/counter.h"

#include "node.h"

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
    std::string uri_;
    int32_t times_;
    int32_t count_;
    bool debug_;
    bool trace_;
    bool counting_;
    json info_;
    std::vector<std::string> classes_;
    Model* model_;
    Engine* engine_;
    BYTETracker tracker_;
    Counter counter_;
};


}  // namespace ma::node