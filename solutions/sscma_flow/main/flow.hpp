#pragma once

#include <string>

#include "img_meter.h"

#include "CGraph.h"
#include "nlohmann/json.hpp"
#include "sscma.h"

using json = nlohmann::json;

struct FlowParam : public CGraph::GElementParam {

    CVoid clone(CGraph::GPassedParamPtr param) override {
        auto* ptr = dynamic_cast<FlowParam*>(param);
        if (nullptr == ptr) [[unlikely]] {
            return;
        }

        this->data = ptr->data;
    }

    json data;
};


struct RequiredParam {
    std::string name;
    std::string type;
    std::string desc;
    std::string default_value;
    std::bool is_optional;
};

class FlowNodeFactory {
public:
    static CGraph::GElementPtr create(const json& config) {
        return std::make_shared<FlowNode>(config);
    }

}

class FlowParamFactory {
public:
    static CGraph::GElementParamPtr create(const json& config) {
        return std::make_shared<FlowParam>(config);
    }
}

class FlowNode {
public:
    explicit FlowNode(const json& config) : config_(config) {}

    static bool isValid(const json& config) {
        // json inputs  = config["inputs"];
        // json outputs = config["outputs"];
        return true;  // TODO: 等到格式定义完毕后再添加
    }

    static json declare() {
        return json{};  // TODO: 等到格式定义完毕后再添加
    }

protected:
    static std::string class_;
    static std::string type_;
    static std::string desc_;
    static std::vector<RequiredParam> input_;
    static std::vector<RequiredParam> output_;
    json config_;
};


class InjectNode : public CGraph::GElement {
public:
    explicit InjectNode(const json& config) : FlowNode(config) {}
};

class FunctionNode : public CGraph::GElement {
public:
    explicit FunctionNode(const json& config) : FlowNode(config) {}
};

class ConditionNode : public CGraph::GCondition {
public:
    explicit ConditionNode(const json& config) : FlowNode(config) {}
}


