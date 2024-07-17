#pragma once

#include <string>

#include "img_meter.h"

#include "CGraph.h"
#include "nlohmann/json.hpp"
#include "sscma.h"

using json = nlohmann::json;

using namespace CGraph;

class FlowNode : public GNode {
public:
    FlowNode(const json& config) : config_(config) {}

protected:
    json config_;
};

class CameraNode : public FlowNode {
public:
    CameraNode(const json& config) : FlowNode(config) {}

    CStatus run() override {
        CStatus status;
        CGRAPH_SLEEP_SECOND(1); // 模拟相机读取数据
        return status;
    }

    CStatus init() override {
        CStatus status;
        return status;
    }
};

class DetectNode : public FlowNode {
public:
    DetectNode(const json& config) : FlowNode(config), detector_(nullptr), engine_(nullptr) {}

    CStatus run() override {
        CStatus status;
        ma_img_t img;
        img.data   = (uint8_t*)gImage_meter;
        img.size   = sizeof(gImage_meter);
        img.width  = 240;
        img.height = 240;
        img.format = MA_PIXEL_FORMAT_RGB888;
        img.rotate = MA_PIXEL_ROTATE_0;
        detector_->run(&img);

        auto perf = detector_->getPerf();
        CGraph::CGRAPH_ECHO("pre: %ldms, infer: %ldms, post: %ldms",
                            perf.preprocess,
                            perf.inference,
                            perf.postprocess);
        auto _results = detector_->getResults();
        int value     = 0;
        for (int i = 0; i < _results.size(); i++) {
            auto result = _results[i];
            CGraph::CGRAPH_ECHO("class: %d, score: %f, x: %f, y: %f, w: %f, h: %f",
                                result.target,
                                result.score,
                                result.x,
                                result.y,
                                result.w,
                                result.h);
            value = value * 10 + result.target;
        }
        return status;
    }


    CStatus init() override {
        CStatus status;
        engine_ = new ma::engine::EngineCVI();
        engine_->init();
        engine_->loadModel(config_["model"]);
        detector_ = new ma::model::YoloV5(engine_);
        return status;
    }

    ~DetectNode() {
        delete detector_;
        delete engine_;
    }

private:
    ma::engine::EngineCVI* engine_;
    ma::model::Detector* detector_;
};


class AddNode : public FlowNode {
public:
    AddNode(const json& config) : FlowNode(config) {}

    CStatus run() override {
        CStatus status;
        // 将 JSON 对象转换为字符串输出
        int a = config_["a"];
        int b = config_["b"];
        std::cout << a + b << std::endl;
        return status;
    }
};

class PrintNode : public FlowNode {
public:
    PrintNode(const json& config) : FlowNode(config) {}

    CStatus run() override {
        CStatus status;
        // 将 JSON 对象转换为字符串输出
        CGraph::CGRAPH_ECHO("%s", config_["content"].dump().c_str());
        return status;
    }
};
