/***************************
@Author: Chunel
@Contact: chunel@foxmail.com
@File: E01-AutoPilot.cpp
@Time: 2023/9/15 20:03
@Desc: 本example主要展示，Camera 每隔1000ms，采集一张图片
       LaneDetector 和 CarDetector同时消费这张图片，计算对应的内容
       并且最终在Show中展示对应的结果信息
***************************/

#include <cstring>
#include <iostream>
#include <memory>

#include "nlohmann/json.hpp"

#include "CGraph.h"

#include "nodes.hpp"

using json = nlohmann::json;

// std::string flow = R"(
// [
//     {
//         "id": "0",
//         "type": "camera",
//         "wires": ["1"]
//     },
//     {
//         "id": "1",
//         "type": "detect",
//         "model": "/mnt/user/model/digital_meter_large_cv181x_int8.cvimodel",
//         "wires": ["2"]
//     },
//     {
//         "id": "2",
//         "type": "print",
//         "content": "Hello World",
//         "x": 840,
//         "y": 1000
//     },
//     {
//         "id": "4",
//         "type": "print",
//         "content": "Is it ok?",
//         "x": 840,
//         "y": 1000
//     },
//     {
//         "id": "5",
//         "type": "print",
//         "content": "Yes",
//         "x": 840,
//         "y": 1000
//     }
// ]
// )";

std::unordered_map<std::string, GElementPtr> element_map;

int main(int argc, char* argv[]) {
    CGraph::CGRAPH_ECHO("main start");

    CGraph::CGRAPH_ECHO("build %s %s", __DATE__, __TIME__);

    const std::string flow_path = argv[1];

    if (flow_path.empty()) {
        CGraph::CGRAPH_ECHO("invalid flow path: %s", flow_path.c_str());
        return 1;
    }

    std::ifstream ifs(flow_path);

    if (!ifs.is_open()) {
        CGraph::CGRAPH_ECHO("invalid flow path: %s", flow_path.c_str());
        return 1;
    }


    auto pipeline = GPipelineFactory::create();

    json j;

    ifs >> j;

    ifs.close();

    for (auto& node : j) {
        CGraph::CGRAPH_ECHO("node: %s", node.dump().c_str());
        const std::string id   = node["id"];
        const std::string type = node["type"];
        const std::string name = type + "_" + id;
        if (node["type"] == "detect") {
            CGraph::CGRAPH_ECHO("type: %s", node["type"].dump().c_str());
            GElementPtr detect = pipeline->createGNode<DetectNode>(GNodeInfo(name, 1), node);
            pipeline->registerGNode(&detect);
            element_map[node["id"]] = detect;
        }

        if (node["type"] == "print") {
            GElementPtr print = pipeline->createGNode<PrintNode>(GNodeInfo(name, 1), node);
            pipeline->registerGNode(&print);
            element_map[node["id"]] = print;
        }

        if (node["type"] == "camera") {
            GElementPtr camera = pipeline->createGNode<CameraNode>(GNodeInfo(name, 1), node);
            pipeline->registerGNode(&camera);
            element_map[node["id"]] = camera;
        }

        if (node["type"] == "add") {
        }
        for (auto& node : j) {
            GElementPtr ele = element_map[node["id"]];
            for (auto wire : node["wires"]) {
                GElementPtr wireEle = element_map[wire];
                CGraph::CGRAPH_ECHO("%s -> %s", ele->getName().c_str(), wireEle->getName().c_str());
                wireEle->addDependGElements({ele});
            }
        }
    }


    pipeline->init();

    while (1) {
        pipeline->run();
    }

    GPipelineFactory::remove(pipeline);

    return 0;
}
