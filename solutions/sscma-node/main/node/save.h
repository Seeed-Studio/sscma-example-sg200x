#pragma once

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
    bool recycle();

protected:
    std::ofstream file_;
    std::string storage_;
    uint64_t max_size_;
    int slice_;
    int duration_;
    bool enabled_;
    ma_tick_t begin_;
    CameraNode* camera_;
    MessageBox frame_;
    Thread* thread_;
};

}  // namespace ma::node