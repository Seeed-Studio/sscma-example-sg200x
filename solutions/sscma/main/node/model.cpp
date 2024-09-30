#include <unistd.h>

#include "model.h"

namespace ma::node {

using namespace ma::engine;
using namespace ma::model;

static constexpr char TAG[] = "ma::node::model";

ModelNode::ModelNode(std::string id)
    : Node("model", id),
      uri_(""),
      debug_(true),
      trace_(false),
      counting_(false),
      count_(0),
      engine_(nullptr),
      model_(nullptr) {}

ModelNode::~ModelNode() {}

ma_err_t ModelNode::onCreate(const json& config) {
    Guard guard(mutex_);
    MA_LOGD(TAG, "create model: %s(%s)", type_.c_str(), id_.c_str());
    return MA_OK;
}

ma_err_t ModelNode::onControl(const std::string& control, const json& data) {
    Guard guard(mutex_);
    MA_LOGD(TAG, "control: %s", control.c_str());
    return MA_OK;
}

ma_err_t ModelNode::onDestroy() {
    Guard guard(mutex_);
    MA_LOGD(TAG, "destroy model: %s(%s)", type_.c_str(), id_.c_str());
    return MA_OK;
}

ma_err_t ModelNode::onStart() {
    Guard guard(mutex_);
    if (started_) {
        return MA_OK;
    }
    MA_LOGD(TAG, "start model: %s(%s)", type_.c_str(), id_.c_str());
    started_ = true;
    return MA_OK;
}

ma_err_t ModelNode::onStop() {
    Guard guard(mutex_);
    if (!started_) {
        return MA_OK;
    }

    MA_LOGD(TAG, "stop model: %s(%s)", type_.c_str(), id_.c_str());

    started_ = false;
    return MA_OK;
}

REGISTER_NODE_SINGLETON("model", ModelNode);

}  // namespace ma::node