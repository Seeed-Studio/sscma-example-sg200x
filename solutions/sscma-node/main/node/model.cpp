/**
 * @file model.cpp
 * @brief ModelNode 类实现
 *
 * 该文件实现了多模型推理节点的完整功能：
 *
 * ==================== 架构概览 ====================
 *
 * 1. 单模型模式（默认）：
 *    - 使用 threadEntry() 作为主推理线程
 *    - 从摄像头获取帧 -> 预处理 -> 推理 -> 结果输出
 *
 * 2. 多模型模式：
 *    - 串行模式（mode="serial"）：
 *      串行执行流程：
 *      ┌─────────┐     ┌─────────┐     ┌─────────┐     ┌─────────┐
 *      │ 摄像头  │────>│ Model1  │────>│ Model2  │────>│ Model3  │────> 输出
 *      │  获取   │     │ (det)   │     │ (cls)   │     │ (seg)   │
 *      └─────────┘     └─────────┘     └─────────┘     └─────────┘
 *           │              │               │               │
 *           └──────────────┴───────────────┴───────────────┘
 *                    单一线程依次执行
 *
 *    - 并行模式（mode="parallel"）：
 *      并行执行流程：
 *      ┌─────────────────────────────────────────────────────────┐
 *      │                      摄像头                             │
 *      │                         │                               │
 *      │           ┌─────────────┼─────────────┐                 │
 *      │           │             │             │                 │
 *      │           ▼             ▼             ▼                 │
 *      │      ┌─────────┐  ┌─────────┐  ┌─────────┐             │
 *      │      │ Model1  │  │ Model2  │  │ Model3  │  独立线程   │
 *      │      └─────────┘  └─────────┘  └─────────┘             │
 *      │           │             │             │                 │
 *      │           └─────────────┴─────────────┘                 │
      └─────────────────────────────────────────────────────────┘
 *
 * ==================== 数据流说明 ====================
 *
 * 串行模式数据传递：
 *   ModelInstance[i].output_frame ──> ModelInstance[i+1].input_frame
 *   使用 Mutex + Event 机制保证线程安全
 *
 * 并行模式数据获取：
 *   每个模型独立从 raw_frame_/jpeg_frame_ 获取摄像头帧
 *   无需数据传递，帧释放由各模型自行管理
 *
 * @note 使用 cv2 命名空间作为 cv 的别名
 */

#include <unistd.h>

#include <opencv2/opencv.hpp>
namespace cv2 = cv;

#include "model.h"

namespace ma::node {

using namespace ma::engine;
using namespace ma::model;

static constexpr char TAG[] = "ma::node::model";

#define DEFAULT_MODEL "/userdata/Models/model.cvimodel"

ModelNode::ModelNode(std::string id)
    : Node("model", id),
      uri_(""),
      debug_(false),
      output_(false),
      trace_(false),
      counting_(false),
      count_(0),
      algorithm_(0),
      engine_(nullptr),
      model_(nullptr),
      thread_(nullptr),
      raw_frame_(1),
      jpeg_frame_(1),
      websocket_(true),
      transport_(nullptr),
      camera_(nullptr),
      preview_width_(640),
      preview_height_(640),
      preview_fps_(30) {}

ModelNode::~ModelNode() {
    onDestroy();
}

/**
 * @brief 单模型模式的主推理线程入口
 *
 * 该函数实现了传统的单模型推理流程：
 * 1. 从摄像头获取原始帧（raw_frame）或调试帧（jpeg_frame）
 * 2. 根据模型输出类型进行推理（检测/分类/关键点/分割）
 * 3. 后处理（坐标转换、跟踪、计数等）
 * 4. 通过WebSocket发送结果
 *
 * @note 此函数仅在单模型模式下使用
 * @note 使用 Thread::enterCritical()/exitCritical() 保护推理过程
 *
 * ==================== 线程循环流程 ====================
 *
 *     ┌─────────────────────────────────────────────┐
 *     │              while (started_)               │
 *     │                                             │
 *     │  ┌───────────────────────────────────────┐  │
 *     │  │ 1. 获取帧                              │  │
 *     │  │    raw_frame_.fetch() / jpeg_frame_   │  │
 *     │  └───────────────────────────────────────┘  │
 *     │                    │                         │
 *     │                    ▼                         │
 *     │  ┌───────────────────────────────────────┐  │
 *     │  │ 2. 检查 enabled 状态                  │  │
 *     │  │    如果禁用，释放帧并继续循环          │  │
 *     │  └───────────────────────────────────────┘  │
 *     │                    │                         │
 *     │                    ▼                         │
 *     │  ┌───────────────────────────────────────┐  │
 *     │  │ 3. 预处理                             │  │
 *     │  │    - 计算缩放比例和偏移               │  │
 *     │  │    - 构建输入张量                      │  │
 *     │  └───────────────────────────────────────┘  │
 *     │                    │                         │
 *     │                    ▼                         │
 *     │  ┌───────────────────────────────────────┐  │
 *     │  │ 4. 模型推理                           │  │
 *     │  │    - Detector.run()                   │  │
 *     │  │    - Classifier.run()                 │  │
 *     │  │    - PoseDetector.run()               │  │
 *     │  │    - Segmentor.run()                  │  │
 *     │  └───────────────────────────────────────┘  │
 *     │                    │                         │
 *     │                    ▼                         │
 *     │  ┌───────────────────────────────────────┐  │
 *     │  │ 5. 后处理                             │  │
 *     │  │    - 坐标反归一化                     │  │
 *     │  │    - BYTETracker 跟踪（可选）         │  │
 *     │  │    - Counter 计数（可选）             │  │
 *     │  └───────────────────────────────────────┘  │
 *     │                    │                         │
 *     │                    ▼                         │
 *     │  ┌───────────────────────────────────────┐  │
 *     │  │ 6. 发送结果                           │  │
 *     │  │    transport_->send()                 │  │
 *     │  └───────────────────────────────────────┘  │
 *     └─────────────────────────────────────────────┘
 */
void ModelNode::threadEntry() {

    ma_err_t err          = MA_OK;
    videoFrame* raw       = nullptr;
    videoFrame* jpeg      = nullptr;
    int32_t width         = 0;
    int32_t height        = 0;
    int32_t target_width  = 0;
    int32_t target_height = 0;
    std::vector<std::string> labels;

    server_->response(id_, json::object({{"type", MA_MSG_TYPE_RESP}, {"name", "enabled"}, {"code", MA_OK}, {"data", enabled_.load()}}));

    while (started_) {

        if (!raw_frame_.fetch(reinterpret_cast<void**>(&raw), Tick::fromSeconds(2))) {
            continue;
        }
        if (debug_ && !jpeg_frame_.fetch(reinterpret_cast<void**>(&jpeg), Tick::fromSeconds(2))) {
            raw->release();
            if (jpeg != nullptr) {
                jpeg->release();
            }
            continue;
        }

        if (!enabled_) {
            raw->release();
            if (debug_) {
                jpeg->release();
            }
            continue;
        }

        Thread::enterCritical();

        ma_tick_t start = Tick::current();

        json reply       = json::object({{"type", MA_MSG_TYPE_EVT}, {"name", "invoke"}, {"code", MA_OK}, {"data", {{"count", ++count_}}}});
        float scale_h    = 1.0;
        float scale_w    = 1.0;
        int32_t offset_x = 0;
        int32_t offset_y = 0;
        if (debug_) {
            width  = jpeg->img.width;
            height = jpeg->img.height;
        } else {
            width  = raw->img.width;
            height = raw->img.height;
        }

        if (width > height) {
            scale_h  = (float)width / (float)height;
            offset_y = (height - width) / 2;
        } else {
            scale_w  = (float)height / (float)width;
            offset_x = (width - height) / 2;
        }

        target_width  = width * scale_w;
        target_height = height * scale_h;

        reply["data"]["resolution"] = json::array({width, height});

        ma_tensor_t tensor = {
            .size        = raw->img.size,
            .is_physical = raw->img.physical,
            .is_variable = false,
        };


        tensor.data.data = reinterpret_cast<void*>(raw->img.data);
        engine_->setInput(0, tensor);
        model_->setPreprocessDone([this, raw](void* ctx) { raw->release(); });

        reply["data"]["labels"] = json::array();

        if (model_->getOutputType() == MA_OUTPUT_TYPE_BBOX) {
            Detector* detector     = static_cast<Detector*>(model_);
            err                    = detector->run(nullptr);
            auto _results          = detector->getResults();
            reply["data"]["boxes"] = json::array();
            std::vector<ma_bbox_t> _bboxes;
            _bboxes.assign(_results.begin(), _results.end());
            if (trace_) {
                auto tracks             = tracker_.inplace_update(_bboxes);
                reply["data"]["tracks"] = tracks;
                for (int i = 0; i < _bboxes.size(); i++) {
                    reply["data"]["boxes"].push_back({static_cast<int16_t>(_bboxes[i].x * target_width + offset_x),
                                                      static_cast<int16_t>(_bboxes[i].y * target_height + offset_y),
                                                      static_cast<int16_t>(_bboxes[i].w * target_width),
                                                      static_cast<int16_t>(_bboxes[i].h * target_height),
                                                      static_cast<int8_t>(_bboxes[i].score * 100),
                                                      _bboxes[i].target});
                    if (labels_.size() > _bboxes[i].target) {
                        reply["data"]["labels"].push_back(labels_[_bboxes[i].target]);
                    } else {
                        reply["data"]["labels"].push_back(std::string("N/A-" + std::to_string(_bboxes[i].target)));
                    }
                    if (counting_) {
                        counter_.update(tracks[i], _bboxes[i].x * 100, _bboxes[i].y * 100);
                    }
                }
                if (counting_ && _bboxes.size() == 0) {
                    counter_.update(-1, 0, 0);
                }
            } else {
                for (int i = 0; i < _bboxes.size(); i++) {
                    reply["data"]["boxes"].push_back({static_cast<int16_t>(_bboxes[i].x * target_width + offset_x),
                                                      static_cast<int16_t>(_bboxes[i].y * target_height + offset_y),
                                                      static_cast<int16_t>(_bboxes[i].w * target_width),
                                                      static_cast<int16_t>(_bboxes[i].h * target_height),
                                                      static_cast<int8_t>(_bboxes[i].score * 100),
                                                      _bboxes[i].target});
                    if (labels_.size() > _bboxes[i].target) {
                        reply["data"]["labels"].push_back(labels_[_bboxes[i].target]);
                    } else {
                        reply["data"]["labels"].push_back(std::string("N/A-" + std::to_string(_bboxes[i].target)));
                    }
                }
            }
            if (counting_) {
                reply["data"]["counts"] = counter_.get();
                reply["data"]["lines"]  = json::array();
                reply["data"]["lines"].push_back(counter_.getSplitter());
            }
        } else if (model_->getOutputType() == MA_OUTPUT_TYPE_CLASS) {
            Classifier* classifier   = static_cast<Classifier*>(model_);
            err                      = classifier->run(nullptr);
            auto _results            = classifier->getResults();
            reply["data"]["classes"] = json::array();
            for (auto& result : _results) {
                reply["data"]["classes"].push_back({static_cast<int8_t>(result.score * 100), result.target});
                if (labels_.size() > result.target) {
                    reply["data"]["labels"].push_back(labels_[result.target]);
                } else {
                    reply["data"]["labels"].push_back(std::string("N/A-" + std::to_string(result.target)));
                }
            }
        } else if (model_->getOutputType() == MA_OUTPUT_TYPE_KEYPOINT) {
            PoseDetector* pose_detector = static_cast<PoseDetector*>(model_);
            err                         = pose_detector->run(nullptr);
            auto _results               = pose_detector->getResults();
            reply["data"]["keypoints"]  = json::array();
            for (auto& result : _results) {
                json pts = json::array();
                for (auto& pt : result.pts) {
                    pts.push_back({static_cast<int16_t>(pt.x * target_width + offset_x), static_cast<int16_t>(pt.y * target_height + offset_y), static_cast<int8_t>(pt.z * 100)});
                }
                json box = {static_cast<int16_t>(result.box.x * target_width + offset_x),
                            static_cast<int16_t>(result.box.y * target_height + offset_y),
                            static_cast<int16_t>(result.box.w * target_width),
                            static_cast<int16_t>(result.box.h * target_height),
                            static_cast<int8_t>(result.box.score * 100),
                            result.box.target};
                if (labels_.size() > result.box.target) {
                    reply["data"]["labels"].push_back(labels_[result.box.target]);
                } else {
                    reply["data"]["labels"].push_back(std::string("N/A-" + std::to_string(result.box.target)));
                }
                reply["data"]["keypoints"].push_back({box, pts});
            }
        } else if (model_->getOutputType() == MA_OUTPUT_TYPE_SEGMENT) {
            Segmentor* segmentor      = static_cast<Segmentor*>(model_);
            err                       = segmentor->run(nullptr);
            auto _results             = segmentor->getResults();
            reply["data"]["segments"] = json::array();
            for (auto& result : _results) {
                json box = {static_cast<int16_t>(result.box.x * target_width + offset_x),
                            static_cast<int16_t>(result.box.y * target_height + offset_y),
                            static_cast<int16_t>(result.box.w * target_width),
                            static_cast<int16_t>(result.box.h * target_height),
                            static_cast<int8_t>(result.box.score * 100),
                            result.box.target};
                if (labels_.size() > result.box.target) {
                    reply["data"]["labels"].push_back(labels_[result.box.target]);
                } else {
                    reply["data"]["labels"].push_back(std::string("N/A-" + std::to_string(result.box.target)));
                }

                cv2::Mat maskImage(result.mask.height, result.mask.width, CV_8UC1, cv2::Scalar(0));

                for (int i = 0; i < result.mask.height; ++i) {
                    for (int j = 0; j < result.mask.width; ++j) {
                        if (result.mask.data[i * result.mask.width / 8 + j / 8] & (1 << (j % 8))) {
                            maskImage.at<uchar>(i, j) = 255;
                        }
                    }
                }

                std::vector<std::vector<cv2::Point>> contours;
                std::vector<cv2::Vec4i> hierarchy;
                cv2::findContours(maskImage, contours, hierarchy, cv2::RETR_EXTERNAL, cv2::CHAIN_APPROX_SIMPLE);

                auto maxContour = std::max_element(contours.begin(), contours.end(), [](std::vector<cv2::Point>& a, std::vector<cv2::Point>& b) { return a.size() < b.size(); });

                std::vector<uint16_t> contour;
                if (maxContour != contours.end()) {
                    contour.reserve(maxContour->size() * 2);
                    float w_scale = width * scale_w / result.mask.width;
                    float h_scale = target_height / result.mask.height;
                    for (auto& c : *maxContour) {
                        contour.push_back(static_cast<uint16_t>(c.x * w_scale + offset_x));
                        contour.push_back(static_cast<uint16_t>(c.y * h_scale + offset_y));
                    }
                }
                reply["data"]["segments"].push_back({box, contour});
            }
        }


        const auto _perf = model_->getPerf();

        reply["data"]["perf"].push_back({_perf.preprocess, _perf.inference, _perf.postprocess});

        if (debug_) {
            char* base64   = new char[4 * ((jpeg->img.size + 2) / 3 + 2)];
            int base64_len = 4 * ((jpeg->img.size + 2) / 3 + 2);
            ma::utils::base64_encode(jpeg->img.data, jpeg->img.size, base64, &base64_len);
            reply["data"]["image"] = std::string(base64, base64_len);
            delete[] base64;
            jpeg->release();
        } else {
            reply["data"]["image"] = "";
        }

        if (websocket_) {
            transport_->send(reinterpret_cast<const char*>(reply.dump().c_str()), reply.dump().size());
        }
        if (!output_) {
            reply["data"]["image"] = "";
        }
        server_->response(id_, reply);

        ma_tick_t end = Tick::current();
        if (debug_ && (end - start < Tick::fromMilliseconds(100))) {
            Thread::sleep(Tick::fromMilliseconds(100) - (end - start));
        }

        Thread::exitCritical();
    }
}

void ModelNode::threadEntryStub(void* obj) {
    reinterpret_cast<ModelNode*>(obj)->threadEntry();
}

void ModelNode::modelInstanceThreadEntryStub(void* obj) {
    ModelInstance* instance = reinterpret_cast<ModelInstance*>(obj);
    reinterpret_cast<ModelNode*>(instance->node)->modelInstanceThreadEntry(instance);
}

/**
 * @brief 模型实例推理线程入口函数
 *
 * 该函数是多模型支持的核心函数，根据运行模式执行不同的逻辑：
 *
 * ==================== 并行模式执行流程 ====================
 *
 *     ┌──────────────────────────────────────────────────────────┐
 *     │                  modelInstanceThreadEntry()              │
 *     │                    每个模型实例独立运行                    │
 *     └──────────────────────────────────────────────────────────┘
 *                                │
 *                                ▼
 *     ┌──────────────────────────────────────────────────────────┐
 *     │  从摄像头获取帧                                            │
 *     │  raw_frame_.fetch() / jpeg_frame_.fetch()                │
 *     │  （每个模型独立获取，互不干扰）                              │
 *     └──────────────────────────────────────────────────────────┘
 *                                │
 *                                ▼
 *     ┌──────────────────────────────────────────────────────────┐
 *     │  输入预处理                                                │
 *     │  preprocessImage() / convertResultToInput()              │
 *     └──────────────────────────────────────────────────────────┘
 *                                │
 *                                ▼
 *     ┌──────────────────────────────────────────────────────────┐
 *     │  模型推理                                                  │
 *     │  Detector/Classifier/PoseDetector/Segmentor.run()        │
 *     └──────────────────────────────────────────────────────────┘
 *                                │
 *                                ▼
 *     ┌──────────────────────────────────────────────────────────┐
 *     │  结果后处理                                                │
 *     │  - 坐标转换                                               │
 *     │  - BYTETracker 跟踪（可选）                               │
 *     │  - Counter 计数（可选）                                   │
 *     └──────────────────────────────────────────────────────────┘
 *                                │
 *                                ▼
 *     ┌──────────────────────────────────────────────────────────┐
 *     │  发送结果                                                  │
 *     │  transport_->send()                                      │
 *     └──────────────────────────────────────────────────────────┘
 *
 * ==================== 串行模式执行流程 ====================
 *
 *     ┌──────────────────────────────────────────────────────────┐
 *     │  Model 1 (is_first=true)                                 │
 *     │  ├─ 从摄像头获取帧                                        │
 *     │  ├─ 预处理 + 推理                                         │
 *     │  └─ 输出 -> output_frame[0]                               │
 *     └──────────────────────────────────────────────────────────┘
 *                                │
 *                                ▼ (通过共享内存传递)
 *     ┌──────────────────────────────────────────────────────────┐
 *     │  Model 2                                                 │
 *     │  ├─ 从 input_frame[1] 获取数据                           │
 *     │  ├─ 预处理 + 推理                                         │
 *     │  └─ 输出 -> output_frame[1]                               │
 *     └──────────────────────────────────────────────────────────┘
 *                                │
 *                                ▼ (通过共享内存传递)
 *     ┌──────────────────────────────────────────────────────────┐
 *     │  Model 3 (is_last=true)                                  │
 *     │  ├─ 从 input_frame[2] 获取数据                           │
 *     │  ├─ 预处理 + 推理                                         │
 *     │  └─ 输出 -> 最终结果                                      │
 *     └──────────────────────────────────────────────────────────┘
 *
 * ==================== 串行模式同步机制 ====================
 *
 * 数据传递使用以下同步机制：
 *
 *   Model[i]                          Model[i+1]
 *      │                                  │
 *      │  1. 写入 output_frame            │
 *      │      mutex.lock()                │
 *      │      *output_frame = result      │
 *      │      data_ready = true           │
 *      │      event.set(1)                │
 *      │      mutex.unlock()              │
 *      │                                  │
 *      │              <── 2. 等待事件 ──   │
 *      │                                  │
 *      │              ──> 3. 读取数据 ──>  │
 *      │                                  │
 *      │  4. 释放 input_frame             │
 *      │      delete input_frame          │
 *      │      input_frame = nullptr       │
 *
 * @param instance 模型实例指针，包含模型配置和状态
 *
 * @note 在并行模式下，所有模型实例的线程同时运行
 * @note 在串行模式下，此函数仅被 serialExecutionEntry 调用
 * @note 使用 Thread::enterCritical()/exitCritical() 保护推理过程
 */
void ModelNode::modelInstanceThreadEntry(ModelInstance* instance) {
    ma_err_t err = MA_OK;
    videoFrame* raw = nullptr;
    videoFrame* jpeg = nullptr;
    int32_t width = 0;
    int32_t height = 0;
    int32_t target_width = 0;
    int32_t target_height = 0;
    
    server_->response(id_, json::object({{"type", MA_MSG_TYPE_RESP}, {"name", "enabled"}, {"code", MA_OK}, {"data", enabled_.load()}}));
    
    while (started_) {
        if (!raw_frame_.fetch(reinterpret_cast<void**>(&raw), Tick::fromSeconds(2))) {
            continue;
        }
        if (instance->debug && !jpeg_frame_.fetch(reinterpret_cast<void**>(&jpeg), Tick::fromSeconds(2))) {
            raw->release();
            if (jpeg != nullptr) {
                jpeg->release();
            }
            continue;
        }
        
        if (!enabled_) {
            raw->release();
            if (instance->debug) {
                jpeg->release();
            }
            continue;
        }
        
        Thread::enterCritical();
        
        ma_tick_t start = Tick::current();
        
        json reply = json::object({{"type", MA_MSG_TYPE_EVT}, {"name", "invoke"}, {"code", MA_OK}, {"data", {{"count", ++count_}}}});
        float scale_h = 1.0;
        float scale_w = 1.0;
        int32_t offset_x = 0;
        int32_t offset_y = 0;
        
        if (instance->debug) {
            width = jpeg->img.width;
            height = jpeg->img.height;
        } else {
            width = raw->img.width;
            height = raw->img.height;
        }
        
        if (width > height) {
            scale_h = (float)width / (float)height;
            offset_y = (height - width) / 2;
        } else {
            scale_w = (float)height / (float)width;
            offset_x = (width - height) / 2;
        }
        
        target_width = width * scale_w;
        target_height = height * scale_h;
        
        reply["data"]["resolution"] = json::array({width, height});
        
        cv2::Mat input_mat;
        
        if (mode_ == "parallel") {
            if (instance->debug) {
                input_mat = cv2::Mat(jpeg->img.height, jpeg->img.width, CV_8UC3, jpeg->img.data);
            } else {
                input_mat = cv2::Mat(raw->img.height, raw->img.width, CV_8UC3, raw->img.data);
            }
            if (!instance->debug) {
                raw->release();
            }
        } else {
            if (!instance->is_first) {
                if (instance->input_frame != nullptr && !instance->input_frame->empty()) {
                    input_mat = *instance->input_frame;
                }
                delete instance->input_frame;
                instance->input_frame = nullptr;
            } else {
                if (instance->debug) {
                    input_mat = cv2::Mat(jpeg->img.height, jpeg->img.width, CV_8UC3, jpeg->img.data);
                } else {
                    input_mat = cv2::Mat(raw->img.height, raw->img.width, CV_8UC3, raw->img.data);
                }
                if (!instance->debug) {
                    raw->release();
                }
            }
        }
        
        cv2::Mat output_mat;
        cv2::Mat processed_mat;
        ma_tensor_t tensor = {0};
        
        if (!input_mat.empty()) {
            output_mat = convertResultToInput(input_mat, instance->model, instance->preprocess_config);
            
            tensor.size = output_mat.cols * output_mat.rows * output_mat.channels() * sizeof(float);
            tensor.is_physical = false;
            tensor.is_variable = false;
            
            tensor.data.f32 = new float[output_mat.cols * output_mat.rows * output_mat.channels()];
            std::memcpy(tensor.data.f32, output_mat.data, tensor.size);
            
            instance->engine->setInput(0, tensor);
        }
        
        auto preprocess_done_callback = [this, tensor](void* ctx) {
            delete[] tensor.data.f32;
        };
        instance->model->setPreprocessDone(preprocess_done_callback);
        
        reply["data"]["labels"] = json::array();
        
        // 根据模型输出类型处理结果
        if (instance->model->getOutputType() == MA_OUTPUT_TYPE_BBOX) {
            Detector* detector = static_cast<Detector*>(instance->model);
            err = detector->run(nullptr);
            auto _results = detector->getResults();
            reply["data"]["boxes"] = json::array();
            std::vector<ma_bbox_t> _bboxes;
            _bboxes.assign(_results.begin(), _results.end());
            
            if (instance->trace) {
                auto tracks = instance->tracker.inplace_update(_bboxes);
                reply["data"]["tracks"] = tracks;
                for (int i = 0; i < _bboxes.size(); i++) {
                    reply["data"]["boxes"].push_back({static_cast<int16_t>(_bboxes[i].x * target_width + offset_x),
                                                      static_cast<int16_t>(_bboxes[i].y * target_height + offset_y),
                                                      static_cast<int16_t>(_bboxes[i].w * target_width),
                                                      static_cast<int16_t>(_bboxes[i].h * target_height),
                                                      static_cast<int8_t>(_bboxes[i].score * 100),
                                                      _bboxes[i].target});
                    if (instance->labels.size() > _bboxes[i].target) {
                        reply["data"]["labels"].push_back(instance->labels[_bboxes[i].target]);
                    } else {
                        reply["data"]["labels"].push_back(std::string("N/A-" + std::to_string(_bboxes[i].target)));
                    }
                    if (instance->counting) {
                        instance->counter.update(tracks[i], _bboxes[i].x * 100, _bboxes[i].y * 100);
                    }
                }
                if (instance->counting && _bboxes.size() == 0) {
                    instance->counter.update(-1, 0, 0);
                }
            } else {
                for (int i = 0; i < _bboxes.size(); i++) {
                    reply["data"]["boxes"].push_back({static_cast<int16_t>(_bboxes[i].x * target_width + offset_x),
                                                      static_cast<int16_t>(_bboxes[i].y * target_height + offset_y),
                                                      static_cast<int16_t>(_bboxes[i].w * target_width),
                                                      static_cast<int16_t>(_bboxes[i].h * target_height),
                                                      static_cast<int8_t>(_bboxes[i].score * 100),
                                                      _bboxes[i].target});
                    if (instance->labels.size() > _bboxes[i].target) {
                        reply["data"]["labels"].push_back(instance->labels[_bboxes[i].target]);
                    } else {
                        reply["data"]["labels"].push_back(std::string("N/A-" + std::to_string(_bboxes[i].target)));
                    }
                }
            }
            if (instance->counting) {
                reply["data"]["counts"] = instance->counter.get();
                reply["data"]["lines"] = json::array();
                reply["data"]["lines"].push_back(instance->counter.getSplitter());
            }
        } else if (instance->model->getOutputType() == MA_OUTPUT_TYPE_CLASS) {
            Classifier* classifier = static_cast<Classifier*>(instance->model);
            err = classifier->run(nullptr);
            auto _results = classifier->getResults();
            reply["data"]["classes"] = json::array();
            for (auto& result : _results) {
                reply["data"]["classes"].push_back({static_cast<int8_t>(result.score * 100), result.target});
                if (instance->labels.size() > result.target) {
                    reply["data"]["labels"].push_back(instance->labels[result.target]);
                } else {
                    reply["data"]["labels"].push_back(std::string("N/A-" + std::to_string(result.target)));
                }
            }
        } else if (instance->model->getOutputType() == MA_OUTPUT_TYPE_KEYPOINT) {
            PoseDetector* pose_detector = static_cast<PoseDetector*>(instance->model);
            err = pose_detector->run(nullptr);
            auto _results = pose_detector->getResults();
            reply["data"]["keypoints"] = json::array();
            for (auto& result : _results) {
                json pts = json::array();
                for (auto& pt : result.pts) {
                    pts.push_back({static_cast<int16_t>(pt.x * target_width + offset_x), static_cast<int16_t>(pt.y * target_height + offset_y), static_cast<int8_t>(pt.z * 100)});
                }
                json box = {static_cast<int16_t>(result.box.x * target_width + offset_x),
                            static_cast<int16_t>(result.box.y * target_height + offset_y),
                            static_cast<int16_t>(result.box.w * target_width),
                            static_cast<int16_t>(result.box.h * target_height),
                            static_cast<int8_t>(result.box.score * 100),
                            result.box.target};
                if (instance->labels.size() > result.box.target) {
                    reply["data"]["labels"].push_back(instance->labels[result.box.target]);
                } else {
                    reply["data"]["labels"].push_back(std::string("N/A-" + std::to_string(result.box.target)));
                }
                reply["data"]["keypoints"].push_back({box, pts});
            }
        } else if (instance->model->getOutputType() == MA_OUTPUT_TYPE_SEGMENT) {
            Segmentor* segmentor = static_cast<Segmentor*>(instance->model);
            err = segmentor->run(nullptr);
            auto _results = segmentor->getResults();
            reply["data"]["segments"] = json::array();
            for (auto& result : _results) {
                json box = {static_cast<int16_t>(result.box.x * target_width + offset_x),
                            static_cast<int16_t>(result.box.y * target_height + offset_y),
                            static_cast<int16_t>(result.box.w * target_width),
                            static_cast<int16_t>(result.box.h * target_height),
                            static_cast<int8_t>(result.box.score * 100),
                            result.box.target};
                if (instance->labels.size() > result.box.target) {
                    reply["data"]["labels"].push_back(instance->labels[result.box.target]);
                } else {
                    reply["data"]["labels"].push_back(std::string("N/A-" + std::to_string(result.box.target)));
                }
                
                cv2::Mat maskImage(result.mask.height, result.mask.width, CV_8UC1, cv2::Scalar(0));
                
                for (int i = 0; i < result.mask.height; ++i) {
                    for (int j = 0; j < result.mask.width; ++j) {
                        if (result.mask.data[i * result.mask.width / 8 + j / 8] & (1 << (j % 8))) {
                            maskImage.at<uchar>(i, j) = 255;
                        }
                    }
                }
                
                std::vector<std::vector<cv2::Point>> contours;
                std::vector<cv2::Vec4i> hierarchy;
                cv2::findContours(maskImage, contours, hierarchy, cv2::RETR_EXTERNAL, cv2::CHAIN_APPROX_SIMPLE);
                
                auto maxContour = std::max_element(contours.begin(), contours.end(), [](std::vector<cv2::Point>& a, std::vector<cv2::Point>& b) { return a.size() < b.size(); });
                
                std::vector<uint16_t> contour;
                if (maxContour != contours.end()) {
                    contour.reserve(maxContour->size() * 2);
                    float w_scale = width * scale_w / result.mask.width;
                    float h_scale = target_height / result.mask.height;
                    for (auto& c : *maxContour) {
                        contour.push_back(static_cast<uint16_t>(c.x * w_scale + offset_x));
                        contour.push_back(static_cast<uint16_t>(c.y * h_scale + offset_y));
                    }
                }
                reply["data"]["segments"].push_back({box, contour});
            }
        }
        
        const auto _perf = instance->model->getPerf();
        reply["data"]["perf"].push_back({_perf.preprocess, _perf.inference, _perf.postprocess});
        
        if (instance->debug) {
            char* base64 = new char[4 * ((jpeg->img.size + 2) / 3 + 2)];
            int base64_len = 4 * ((jpeg->img.size + 2) / 3 + 2);
            ma::utils::base64_encode(jpeg->img.data, jpeg->img.size, base64, &base64_len);
            reply["data"]["image"] = std::string(base64, base64_len);
            delete[] base64;
            jpeg->release();
        } else {
            reply["data"]["image"] = "";
        }
        
        if (instance->websocket && instance->transport != nullptr) {
            instance->transport->send(reinterpret_cast<const char*>(reply.dump().c_str()), reply.dump().size());
        }
        if (!instance->output) {
            reply["data"]["image"] = "";
        }
        
        if (mode_ == "serial" && !instance->is_last) {
            if (!output_mat.empty()) {
                cv2::Mat result_mat;
                output_mat.convertTo(result_mat, CV_8UC3, 255.0);
                if (instance->preprocess_config.input_format == "RGB" || instance->preprocess_config.swap_rb) {
                    cv2::cvtColor(result_mat, result_mat, cv2::COLOR_RGB2BGR);
                }
                if (instance->output_frame != nullptr) {
                    delete instance->output_frame;
                }
                instance->output_frame = new cv2::Mat(result_mat);
            }
        }
        
        server_->response(id_, reply);
        
        ma_tick_t end = Tick::current();
        if (instance->debug && (end - start < Tick::fromMilliseconds(100))) {
            Thread::sleep(Tick::fromMilliseconds(100) - (end - start));
        }
        
        Thread::exitCritical();
    }
}

void ModelNode::serialExecutionEntryStub(void* obj) {
    reinterpret_cast<ModelNode*>(obj)->serialExecutionEntry();
}

/**
 * @brief 串行执行模式的主线程入口函数
 *
 * 串行模式在单一线程中依次执行所有模型实例，每个模型的输出作为下一个模型的输入。
 *
 * ==================== 串行执行数据流 ====================
 *
 *     ┌─────────────────────────────────────────────────────────────────┐
 *     │                        serialExecutionEntry()                   │
 *     │                         单一线程执行全部                         │
 *     └─────────────────────────────────────────────────────────────────┘
 *                                    │
 *                                    ▼
 *     ┌─────────────────────────────────────────────────────────────────┐
 *     │  步骤1: 从摄像头获取原始帧                                        │
 *     │  raw_frame_.fetch(&raw)                                         │
 *     │  - 创建 current_frame Mat                                        │
 *     │  - 释放 raw 帧资源                                               │
 *     └─────────────────────────────────────────────────────────────────┘
 *                                    │
 *                                    ▼
 *     ┌─────────────────────────────────────────────────────────────────┐
 *     │  步骤2: 遍历所有模型实例，按顺序执行                              │
 *     │                                                                 │
 *     │  for (i = 0; i < models_.size(); ++i)                          │
 *     │      │                                                          │
 *     │      ▼                                                          │
 *     │  ┌───────────────────────────────────────────────────────────┐  │
 *     │  │ Model 1 (is_first=true)                                   │  │
 *     │  │ ├─ input_mat = current_frame.clone()                      │  │
 *     │  │ ├─ preprocessImage() → processed_mat                      │  │
 *     │  │ ├─ 创建 tensor 并设置输入                                   │  │
 *     │  │ ├─ model->run() 推理                                       │  │
 *     │  │ ├─ 处理结果 → instance_reply                               │  │
 *     │  │ └─ 传递输入给下一个模型                                      │  │
 *     │  └───────────────────────────────────────────────────────────┘  │
 *     │                              │                                  │
 *     │                              ▼                                  │
 *     │  ┌───────────────────────────────────────────────────────────┐  │
 *     │  │ Model 2                                                   │  │
 *     │  │ ├─ 等待上一模型信号 (event.wait())                         │  │
 *     │  │ ├─ 从 input_frame 获取数据                                 │  │
 *     │  │ ├─ preprocessImage() → processed_mat                      │  │
 *     │  │ ├─ model->run() 推理                                       │  │
 *     │  │ ├─ 处理结果 → instance_reply                               │  │
 *     │  │ └─ 传递输入给下一个模型                                      │  │
 *     │  └───────────────────────────────────────────────────────────┘  │
 *     │                              │                                  │
 *     │                              ▼                                  │
 *     │  ┌───────────────────────────────────────────────────────────┐  │
 *     │  │ Model 3 (is_last=true)                                    │  │
 *     │  │ ├─ 等待上一模型信号 (event.wait())                         │  │
 *     │  │ ├─ 从 input_frame 获取数据                                 │  │
 *     │  │ ├─ preprocessImage() → processed_mat                      │  │
 *     │  │ ├─ model->run() 推理                                       │  │
 *     │  │ └─ 处理结果 → instance_reply (最终输出)                     │  │
 *     │  └───────────────────────────────────────────────────────────┘  │
 *     └─────────────────────────────────────────────────────────────────┘
 *                                    │
 *                                    ▼
 *     ┌─────────────────────────────────────────────────────────────────┐
 *     │  步骤3: 汇总结果并发送                                           │
 *     │  - 合并所有模型的结果到 reply                                    │
 *     │  - 通过 WebSocket 发送 (如果启用)                                │
 *     │  - server_->response() 响应                                     │
 *     └─────────────────────────────────────────────────────────────────┘
 *
 * ==================== 模型间数据传递机制 ====================
 *
 *   Model[i] 处理完成                    Model[i+1] 开始处理
 *         │                                    │
 *         │  1. 复制输入数据                    │
 *         │     next->mutex.lock()             │
 *         │     next->input_frame = new Mat()  │
 *         │                                    │
 *         │  2. 设置同步信号                    │
 *         │     next->data_ready = true        │
 *         │     next->event.set(1)             │
 *         │     next->mutex.unlock()           │
 *         │                                    │
 *         │              <── 阻塞等待 ──        │
 *         │                                    │
 *         │              ──> 获取数据 ──>       │
 *         │                 input_mat          │
 *         │                                    │
 *         │  3. 释放资源                        │
 *         │     delete input_frame             │
 *         │     input_frame = nullptr          │
 *
 * ==================== 与并行模式的对比 ====================
 *
 * | 特性         | 串行模式 (serial)          | 并行模式 (parallel)        |
 * |--------------|----------------------------|----------------------------|
 * | 线程数量     | 1个主线程                  | N个模型线程 (N=模型数量)   |
 * | 数据来源     | 模型间传递                 | 每个模型独立获取摄像头帧    |
 * | 同步机制     | Mutex + Event              | 无需同步                   |
 * | 适用场景     | 模型级联 (检测→分类→分割)   | 多模型独立推理              |
 * | 内存使用     | 共享 input_frame 缓冲区    | 各自独立处理               |
 * | 延迟         | 累加 (每帧处理时间求和)    | 最大值 (并行处理取最大)    |
 *
 * @note 此函数仅在 mode_ == "serial" 时被调用
 * @note 使用 Thread::enterCritical()/exitCritical() 保护整个处理流程
 * @note 每个模型的推理结果存储在 reply["data"]["results"][model_id] 中
 */
void ModelNode::serialExecutionEntry() {

    ma_err_t err = MA_OK;
    videoFrame* raw = nullptr;
    videoFrame* jpeg = nullptr;

    server_->response(id_, json::object({{"type", MA_MSG_TYPE_RESP}, {"name", "enabled"}, {"code", MA_OK}, {"data", enabled_.load()}}));

    while (started_) {
        cv2::Mat current_frame;

        if (!raw_frame_.fetch(reinterpret_cast<void**>(&raw), Tick::fromSeconds(2))) {
            continue;
        }

        if (!enabled_) {
            raw->release();
            continue;
        }

        Thread::enterCritical();
        ma_tick_t start = Tick::current();

        json reply = json::object({{"type", MA_MSG_TYPE_EVT}, {"name", "invoke"}, {"code", MA_OK}, {"data", {{"count", ++count_}}}});
        reply["data"]["perf"] = json::array();

        int32_t width = raw->img.width;
        int32_t height = raw->img.height;
        int32_t target_width = width, target_height = height;
        reply["data"]["resolution"] = json::array({width, height});

        current_frame = cv2::Mat(raw->img.height, raw->img.width, CV_8UC3, raw->img.data);
        raw->release();

        for (size_t i = 0; i < models_.size(); ++i) {
            auto instance = models_[i];

            cv2::Mat input_mat;
            cv2::Mat output_mat;

            if (instance->is_first) {
                input_mat = current_frame.clone();
            } else {
                instance->mutex.lock();
                while (!instance->data_ready && started_) {
                    instance->event.wait(0, nullptr, Tick::fromMilliseconds(100));
                }
                if (instance->data_ready && instance->input_frame != nullptr && !instance->input_frame->empty()) {
                    input_mat = *instance->input_frame;
                    delete instance->input_frame;
                    instance->input_frame = nullptr;
                }
                instance->data_ready = false;
                instance->event.clear();
                instance->mutex.unlock();
            }

            if (!input_mat.empty()) {
                cv2::Mat processed_mat = preprocessImage(input_mat, instance->preprocess_config);

                ma_tensor_t tensor = {0};
                tensor.size = processed_mat.cols * processed_mat.rows * processed_mat.channels() * sizeof(float);
                tensor.is_physical = false;
                tensor.is_variable = false;
                tensor.data.f32 = new float[processed_mat.cols * processed_mat.rows * processed_mat.channels()];
                std::memcpy(tensor.data.f32, processed_mat.data, tensor.size);

                instance->engine->setInput(0, tensor);

                auto preprocess_done_callback = [tensor](void* ctx) {
                    delete[] tensor.data.f32;
                };
                instance->model->setPreprocessDone(preprocess_done_callback);

                json instance_reply = json::object();
                instance_reply["model_id"] = instance->id;
                instance_reply["model_name"] = instance->name;

                if (instance->model->getOutputType() == MA_OUTPUT_TYPE_BBOX) {
                    Detector* detector = static_cast<Detector*>(instance->model);
                    err = detector->run(nullptr);
                    auto _results = detector->getResults();
                    instance_reply["boxes"] = json::array();
                    std::vector<ma_bbox_t> _bboxes;
                    _bboxes.assign(_results.begin(), _results.end());

                    float scale_h = 1.0, scale_w = 1.0;
                    int32_t offset_x = 0, offset_y = 0;

                    if (instance->trace) {
                        auto tracks = instance->tracker.inplace_update(_bboxes);
                        instance_reply["tracks"] = tracks;
                        for (int j = 0; j < _bboxes.size(); j++) {
                            instance_reply["boxes"].push_back({static_cast<int16_t>(_bboxes[j].x * target_width + offset_x),
                                                              static_cast<int16_t>(_bboxes[j].y * target_height + offset_y),
                                                              static_cast<int16_t>(_bboxes[j].w * target_width),
                                                              static_cast<int16_t>(_bboxes[j].h * target_height),
                                                              static_cast<int8_t>(_bboxes[j].score * 100),
                                                              _bboxes[j].target});
                        }
                    } else {
                        for (int j = 0; j < _bboxes.size(); j++) {
                            instance_reply["boxes"].push_back({static_cast<int16_t>(_bboxes[j].x * target_width + offset_x),
                                                              static_cast<int16_t>(_bboxes[j].y * target_height + offset_y),
                                                              static_cast<int16_t>(_bboxes[j].w * target_width),
                                                              static_cast<int16_t>(_bboxes[j].h * target_height),
                                                              static_cast<int8_t>(_bboxes[j].score * 100),
                                                              _bboxes[j].target});
                        }
                    }
                } else if (instance->model->getOutputType() == MA_OUTPUT_TYPE_CLASS) {
                    Classifier* classifier = static_cast<Classifier*>(instance->model);
                    err = classifier->run(nullptr);
                    auto _results = classifier->getResults();
                    instance_reply["classes"] = json::array();
                    for (auto& result : _results) {
                        instance_reply["classes"].push_back({static_cast<int8_t>(result.score * 100), result.target});
                    }
                } else if (instance->model->getOutputType() == MA_OUTPUT_TYPE_KEYPOINT) {
                    PoseDetector* pose_detector = static_cast<PoseDetector*>(instance->model);
                    err = pose_detector->run(nullptr);
                    auto _results = pose_detector->getResults();
                    instance_reply["keypoints"] = json::array();
                    for (auto& result : _results) {
                        json pts = json::array();
                        for (auto& pt : result.pts) {
                            pts.push_back({static_cast<int16_t>(pt.x * target_width), static_cast<int16_t>(pt.y * target_height), static_cast<int8_t>(pt.z * 100)});
                        }
                        json box = {static_cast<int16_t>(result.box.x * target_width),
                                    static_cast<int16_t>(result.box.y * target_height),
                                    static_cast<int16_t>(result.box.w * target_width),
                                    static_cast<int16_t>(result.box.h * target_height),
                                    static_cast<int8_t>(result.box.score * 100),
                                    result.box.target};
                        instance_reply["keypoints"].push_back({box, pts});
                    }
                } else if (instance->model->getOutputType() == MA_OUTPUT_TYPE_SEGMENT) {
                    Segmentor* segmentor = static_cast<Segmentor*>(instance->model);
                    err = segmentor->run(nullptr);
                    auto _results = segmentor->getResults();
                    instance_reply["segments"] = json::array();
                    for (auto& result : _results) {
                        json box = {static_cast<int16_t>(result.box.x * width),
                                    static_cast<int16_t>(result.box.y * height),
                                    static_cast<int16_t>(result.box.w * width),
                                    static_cast<int16_t>(result.box.h * height),
                                    static_cast<int8_t>(result.box.score * 100),
                                    result.box.target};
                        instance_reply["segments"].push_back({box});
                    }
                }

                const auto _perf = instance->model->getPerf();
                reply["data"]["perf"].push_back({_perf.preprocess, _perf.inference, _perf.postprocess});

                reply["data"]["results"][instance->id] = instance_reply;

                if (instance->websocket && instance->transport != nullptr) {
                    instance->transport->send(reinterpret_cast<const char*>(reply.dump().c_str()), reply.dump().size());
                }

                if (!instance->is_last && !input_mat.empty()) {
                    if (i + 1 < models_.size()) {
                        auto next_instance = models_[i + 1];
                        next_instance->mutex.lock();
                        if (next_instance->input_frame != nullptr) {
                            delete next_instance->input_frame;
                        }
                        next_instance->input_frame = new cv2::Mat(input_mat);
                        next_instance->data_ready = true;
                        next_instance->event.set(1);
                        next_instance->mutex.unlock();
                    }
                }
            }
        }

        if (!output_) {
            reply["data"]["image"] = "";
        }

        server_->response(id_, reply);

        ma_tick_t end = Tick::current();
        if (end - start < Tick::fromMilliseconds(100)) {
            Thread::sleep(Tick::fromMilliseconds(100) - (end - start));
        }

        Thread::exitCritical();
    }
}

/**
 * @brief 创建模型节点
 *
 * 根据配置初始化模型节点，支持单模型和多模型两种模式。
 * 该函数负责加载模型文件、初始化引擎、创建模型实例、配置预处理参数等。
 *
 * ==================== 函数执行流程 ====================
 *
 *     ┌─────────────────────────────────────────────────────────────────┐
 *     │                      onCreate(config)                          │
 *     │                     输入: JSON配置                              │
 *     └─────────────────────────────────────────────────────────────────┘
 *                                    │
 *                                    ▼
 *     ┌─────────────────────────────────────────────────────────────────┐
 *     │  步骤1: 清理现有资源                                             │
 *     │  ├─ 删除所有已存在的 ModelInstance                              │
 *     │  └─ 清空 models_ 向量                                           │
 *     └─────────────────────────────────────────────────────────────────┘
 *                                    │
 *                                    ▼
 *     ┌─────────────────────────────────────────────────────────────────┐
 *     │  步骤2: 读取全局配置                                             │
 *     │  ├─ mode_ = config["mode"] ("serial" 或 "parallel")            │
 *     │  └─ batch_size_ = config["batch_size"] (默认1)                 │
 *     └─────────────────────────────────────────────────────────────────┘
 *                                    │
 *                                    ▼
 *     ┌─────────────────────────────────────────────────────────────────┐
 *     │  步骤3: 判断配置模式                                             │
 *     │                                                                 │
 *     │  config.contains("models")?                                    │
 *     │      │                                                         │
 *     │      ├─ YES → 多模型模式                                        │
 *     │      │     └─ 遍历 models 数组加载每个模型                       │
 *     │      │                                                         │
 *     │      └─ NO  → 单模型模式 (向后兼容)                              │
 *     │            └─ 从顶层配置加载单个模型                             │
 *     └─────────────────────────────────────────────────────────────────┘
 *                                    │
 *                    ┌───────────────┴───────────────┐
 *                    ▼                               ▼
 *     ┌─────────────────────────┐   ┌───────────────────────────────────┐
 *     │   多模型加载流程         │   │     单模型加载流程                 │
 *     ├─────────────────────────┤   ├───────────────────────────────────┤
 *     │ for each model_config:  │   │  加载 uri, algorithm 等配置        │
     * │   ├─ 创建 ModelInstance  │   │                                   │
 *     │   ├─ 加载模型文件        │   │  加载模型文件 (与多模型相同)        │
 *     │   ├─ 创建 Engine        │   │                                   │
 *     │   ├─ 创建 Model         │   │  创建 Engine 和 Model              │
 *     │   ├─ 设置模型参数        │   │                                   │
 *     │   ├─ 加载预处理配置      │   │  设置模型参数                      │
 *     │   └─ 配置 WebSocket      │   │                                   │
 *     │                         │   │  加载预处理配置                     │
 *     │   models_.push_back()   │   │                                   │
 *     │                         │   │  配置 WebSocket                    │
 *     └─────────────────────────┘   │                                   │
 *                                   │  models_.push_back()               │
 *                                   └───────────────────────────────────┘
 *                                    │
 *                                    ▼
 *     ┌─────────────────────────────────────────────────────────────────┐
 *     │  步骤4: 返回结果                                                 │
 *     │  ├─ created_ = true                                            │
 *     │  └─ 发送 response 响应                                          │
 *     └─────────────────────────────────────────────────────────────────┘
 *
 * ==================== 多模型 vs 单模型配置对比 ====================
 *
 * | 配置项       | 多模型模式 (models数组)         | 单模型模式 (顶层配置)       |
 * |--------------|--------------------------------|----------------------------|
 * | 模型来源     | config["models"][i]["uri"]     | config["uri"]              |
 * | 算法类型     | config["models"][i]["algorithm"] | config["algorithm"]       |
 * | 标签         | config["models"][i]["labels"]  | config["labels"]           |
 * | 阈值         | config["models"][i]["tscore"]  | config["tscore"]           |
 * | 调试模式     | config["models"][i]["debug"]   | config["debug"]            |
 * | 预处理       | config["models"][i]["preprocess"] | 不支持                   |
 * | WebSocket    | config["models"][i]["websocket"] | config["websocket"]       |
 * | 执行模式     | 由顶层 mode_ 控制              | 由顶层 mode_ 控制          |
 *
 * ==================== JSON配置示例 ====================
 *
 * 多模型配置 (mode="serial"):
 * {
 *     "mode": "serial",
 *     "models": [
 *         {
 *             "id": "detector",
 *             "name": "yolov8n",
 *             "uri": "/path/to/detector.smodel",
 *             "preprocess": {
 *                 "input_width": 640,
 *                 "input_height": 640,
 *                 "input_format": "RGB",
 *                 "letterbox": true
 *             }
 *         },
 *         {
 *             "id": "classifier",
 *             "name": "mobilenetv3",
 *             "uri": "/path/to/classifier.smodel",
 *             "preprocess": {
 *                 "input_width": 224,
 *                 "input_height": 224,
 *                 "input_format": "RGB"
 *             }
 *         }
 *     ]
 * }
 *
 * 单模型配置 (向后兼容):
 * {
 *     "uri": "/path/to/model.smodel",
 *     "algorithm": 1,
 *     "tscore": 0.5,
 *     "debug": true
 * }
 *
 * @param config JSON配置对象，包含模型配置参数
 * @return ma_err_t 错误码，MA_OK表示成功
 *
 * @note 如果 config["mode"] 不存在，默认使用 "serial" (串行模式)
 * @note 多模型模式要求 config["models"] 是一个数组
 * @note 每个模型可以独立配置预处理参数 preprocess_config
 * @see ModelInstance 模型实例结构体定义
 * @see onStart 启动模型推理
 * @see PreprocessConfig 预处理配置结构体
 */
ma_err_t ModelNode::onCreate(const json& config) {
    ma_err_t err = MA_OK;
    Guard guard(mutex_);

    // 清空现有模型
    for (auto instance : models_) {
        if (instance->engine != nullptr) {
            delete instance->engine;
        }
        if (instance->model != nullptr) {
            delete instance->model;
        }
        if (instance->thread != nullptr) {
            delete instance->thread;
        }
        if (instance->transport != nullptr) {
            delete instance->transport;
        }
        delete instance;
    }
    models_.clear();

    // 设置运行模式和批处理大小
    mode_ = config.value("mode", "serial");
    batch_size_ = config.value("batch_size", 1);

    // 加载多个模型配置
    if (config.contains("models") && config["models"].is_array()) {
        const auto& models_config = config["models"];
        for (size_t i = 0; i < models_config.size(); ++i) {
            const auto& model_config = models_config[i];
            std::string model_id = model_config.value("id", "model_" + std::to_string(i));
            std::string model_name = model_config.value("name", "model_" + std::to_string(i));
            
            auto instance = new ModelInstance(model_id, model_name);
            
            // 模型路径
            std::string model_uri = model_config.value("uri", DEFAULT_MODEL);
            if (access(model_uri.c_str(), R_OK) != 0) {
                MA_THROW(Exception(MA_ENOENT, "Model file not found " + model_uri));
            }
            
            // 模型算法类型
            int model_algorithm = model_config.value("algorithm", algorithm_);
            
            // 查找model.json
            size_t pos = model_uri.find_last_of(".");
            if (pos != std::string::npos) {
                std::string path = model_uri.substr(0, pos) + ".json";
                if (access(path.c_str(), R_OK) == 0) {
                    std::ifstream ifs(path);
                    if (!ifs.is_open()) {
                        MA_THROW(Exception(MA_EINVAL, "Model config file not found " + path));
                    }
                    ifs >> instance->info;
                    if (instance->info.is_object()) {
                        if (instance->info.contains("classes") && instance->info["classes"].is_array()) {
                            instance->labels = instance->info["classes"].get<std::vector<std::string>>();
                        }
                    }
                }
            }
            
            // 覆盖模型标签
            if (instance->labels.size() == 0 && model_config.contains("labels") && model_config["labels"].is_array() && model_config["labels"].size() > 0) {
                instance->labels = model_config["labels"].get<std::vector<std::string>>();
            }
            
            // 初始化引擎和模型
            MA_TRY {
                instance->engine = new EngineDefault();
                if (instance->engine == nullptr) {
                    MA_THROW(Exception(MA_ENOMEM, "Engine init failed"));
                }
                if (instance->engine->init() != MA_OK) {
                    MA_THROW(Exception(MA_EINVAL, "Engine init failed"));
                }
                if (instance->engine->load(model_uri) != MA_OK) {
                    MA_THROW(Exception(MA_EINVAL, "Engine load failed"));
                }
                instance->model = ModelFactory::create(instance->engine, model_algorithm);
                if (instance->model == nullptr) {
                    MA_THROW(Exception(MA_ENOTSUP, "Model not supported"));
                }
                
                MA_LOGI(TAG, "model: %s %s", model_uri.c_str(), instance->model->getName());
                
                // 模型特定配置
                if (model_config.contains("tscore")) {
                    instance->model->setConfig(MA_MODEL_CFG_OPT_THRESHOLD, model_config["tscore"].get<float>());
                }
                if (model_config.contains("tiou")) {
                    instance->model->setConfig(MA_MODEL_CFG_OPT_NMS, model_config["tiou"].get<float>());
                }
                if (model_config.contains("topk")) {
                    instance->model->setConfig(MA_MODEL_CFG_OPT_TOPK, model_config["topk"].get<int32_t>());
                }
                if (model_config.contains("debug")) {
                    instance->output = model_config["debug"].get<bool>();
                }
                if (model_config.contains("websocket") && model_config["websocket"].is_boolean()) {
                    instance->websocket = model_config["websocket"].get<bool>();
                }
                if (instance->websocket || instance->output) {
                    instance->debug = true;
                }
                if (model_config.contains("trace")) {
                    instance->trace = model_config["trace"].get<bool>();
                }
                if (model_config.contains("counting")) {
                    instance->counting = model_config["counting"].get<bool>();
                }
                if (model_config.contains("splitter") && model_config["splitter"].is_array()) {
                    instance->counter.setSplitter(model_config["splitter"].get<std::vector<int16_t>>());
                }
                if (model_config.contains("previewResolution") && model_config["previewResolution"].is_string()) {
                    if (model_config["previewResolution"].get<std::string>() == "auto") {
                        instance->preview_width = -1;
                        instance->preview_height = -1;
                    } else {
                        std::string resolution = model_config["previewResolution"].get<std::string>();
                        size_t pos = resolution.find('x');
                        if (pos != std::string::npos) {
                            instance->preview_width = std::stoi(resolution.substr(0, pos));
                            instance->preview_height = std::stoi(resolution.substr(pos + 1));
                        }
                    }
                }
                if (model_config.contains("previewFps") && model_config["previewFps"].is_number_integer()) {
                    instance->preview_fps = model_config["previewFps"].get<int32_t>();
                }
                
                // 加载预处理配置
                if (model_config.contains("preprocess")) {
                    const auto& preprocess_config = model_config["preprocess"];
                    if (preprocess_config.contains("input_width")) {
                        instance->preprocess_config.input_width = preprocess_config["input_width"].get<int>();
                    }
                    if (preprocess_config.contains("input_height")) {
                        instance->preprocess_config.input_height = preprocess_config["input_height"].get<int>();
                    }
                    if (preprocess_config.contains("input_format")) {
                        instance->preprocess_config.input_format = preprocess_config["input_format"].get<std::string>();
                    }
                    if (preprocess_config.contains("mean") && preprocess_config["mean"].is_array()) {
                        instance->preprocess_config.mean = preprocess_config["mean"].get<std::vector<float>>();
                    }
                    if (preprocess_config.contains("std") && preprocess_config["std"].is_array()) {
                        instance->preprocess_config.std = preprocess_config["std"].get<std::vector<float>>();
                    }
                    if (preprocess_config.contains("swap_rb")) {
                        instance->preprocess_config.swap_rb = preprocess_config["swap_rb"].get<bool>();
                    }
                    if (preprocess_config.contains("normalize")) {
                        instance->preprocess_config.normalize = preprocess_config["normalize"].get<bool>();
                    }
                    if (preprocess_config.contains("letterbox")) {
                        instance->preprocess_config.letterbox = preprocess_config["letterbox"].get<bool>();
                    }
                }
                
                // WebSocket配置
                if (instance->websocket) {
                    TransportWebSocket::Config ws_config = {.port = 8090};
                    MA_STORAGE_GET_POD(server_->getStorage(), MA_STORAGE_KEY_WS_PORT, ws_config.port, 8090);
                    instance->transport = new TransportWebSocket();
                    if (instance->transport != nullptr) {
                        instance->transport->init(&ws_config);
                    }
                    MA_LOGI(TAG, "camera websocket server started on port %d", ws_config.port);
                } else {
                    instance->transport = nullptr;
                }
                
                models_.push_back(instance);
            } MA_CATCH(ma::Exception & e) {
                if (instance->engine != nullptr) {
                    delete instance->engine;
                    instance->engine = nullptr;
                }
                if (instance->model != nullptr) {
                    delete instance->model;
                    instance->model = nullptr;
                }
                if (instance->thread != nullptr) {
                    delete instance->thread;
                    instance->thread = nullptr;
                }
                if (instance->transport != nullptr) {
                    delete instance->transport;
                    instance->transport = nullptr;
                }
                delete instance;
                MA_THROW(e);
            } MA_CATCH(std::exception & e) {
                if (instance->engine != nullptr) {
                    delete instance->engine;
                    instance->engine = nullptr;
                }
                if (instance->model != nullptr) {
                    delete instance->model;
                    instance->model = nullptr;
                }
                if (instance->thread != nullptr) {
                    delete instance->thread;
                    instance->thread = nullptr;
                }
                if (instance->transport != nullptr) {
                    delete instance->transport;
                    instance->transport = nullptr;
                }
                delete instance;
                MA_THROW(Exception(MA_EINVAL, e.what()));
            }
        }
    } else {
        // 向后兼容：加载单个模型
        labels_.clear();
        
        if (config.contains("algorithm") && config["algorithm"].is_number_integer()) {
            algorithm_ = config["algorithm"].get<int>();
        }
        
        if (config.contains("uri") && config["uri"].is_string()) {
            uri_ = config["uri"].get<std::string>();
        }
        
        if (uri_.empty()) {
            uri_ = DEFAULT_MODEL;
        }
        
        if (access(uri_.c_str(), R_OK) != 0) {
            MA_THROW(Exception(MA_ENOENT, "Model file not found " + uri_));
        }
        
        // find model.json
        size_t pos = uri_.find_last_of(".");
        if (pos != std::string::npos) {
            std::string path = uri_.substr(0, pos) + ".json";
            if (access(path.c_str(), R_OK) == 0) {
                std::ifstream ifs(path);
                if (!ifs.is_open()) {
                    MA_THROW(Exception(MA_EINVAL, "Model config file not found " + path));
                }
                ifs >> info_;
                if (info_.is_object()) {
                    if (info_.contains("classes") && info_["classes"].is_array()) {
                        labels_ = info_["classes"].get<std::vector<std::string>>();
                    }
                }
            }
        }
        
        // override classes
        if (labels_.size() == 0 && config.contains("labels") && config["labels"].is_array() && config["labels"].size() > 0) {
            labels_ = config["labels"].get<std::vector<std::string>>();
        }
        
        auto instance = new ModelInstance("default", "default");
        instance->labels = labels_;
        instance->info = info_;
        
        MA_TRY {
            instance->engine = new EngineDefault();
            if (instance->engine == nullptr) {
                MA_THROW(Exception(MA_ENOMEM, "Engine init failed"));
            }
            if (instance->engine->init() != MA_OK) {
                MA_THROW(Exception(MA_EINVAL, "Engine init failed"));
            }
            if (instance->engine->load(uri_) != MA_OK) {
                MA_THROW(Exception(MA_EINVAL, "Engine load failed"));
            }
            instance->model = ModelFactory::create(instance->engine, algorithm_);
            if (instance->model == nullptr) {
                MA_THROW(Exception(MA_ENOTSUP, "Model not supported"));
            }
            
            MA_LOGI(TAG, "model: %s %s", uri_.c_str(), instance->model->getName());
            
            // extra config
            if (config.contains("tscore")) {
                instance->model->setConfig(MA_MODEL_CFG_OPT_THRESHOLD, config["tscore"].get<float>());
            }
            if (config.contains("tiou")) {
                instance->model->setConfig(MA_MODEL_CFG_OPT_NMS, config["tiou"].get<float>());
            }
            if (config.contains("topk")) {
                instance->model->setConfig(MA_MODEL_CFG_OPT_TOPK, config["tiou"].get<float>());
            }
            if (config.contains("debug")) {
                instance->output = config["debug"].get<bool>();
            }
            if (config.contains("websocket") && config["websocket"].is_boolean()) {
                instance->websocket = config["websocket"].get<bool>();
            }
            if (instance->websocket || instance->output) {
                instance->debug = true;
            }
            if (config.contains("trace")) {
                instance->trace = config["trace"].get<bool>();
            }
            if (config.contains("counting")) {
                instance->counting = config["counting"].get<bool>();
            }
            if (config.contains("splitter") && config["splitter"].is_array()) {
                instance->counter.setSplitter(config["splitter"].get<std::vector<int16_t>>());
            }
            if (config.contains("previewResolution") && config["previewResolution"].is_string()) {
                if (config["previewResolution"].get<std::string>() == "auto") {
                    instance->preview_width = -1;
                    instance->preview_height = -1;
                } else {
                    std::string resolution = config["previewResolution"].get<std::string>();
                    size_t pos = resolution.find('x');
                    if (pos != std::string::npos) {
                        instance->preview_width = std::stoi(resolution.substr(0, pos));
                        instance->preview_height = std::stoi(resolution.substr(pos + 1));
                    }
                }
            }
            if (config.contains("previewFps") && config["previewFps"].is_number_integer()) {
                instance->preview_fps = config["previewFps"].get<int32_t>();
            }
            
            if (instance->websocket) {
                TransportWebSocket::Config ws_config = {.port = 8090};
                MA_STORAGE_GET_POD(server_->getStorage(), MA_STORAGE_KEY_WS_PORT, ws_config.port, 8090);
                instance->transport = new TransportWebSocket();
                if (instance->transport != nullptr) {
                    instance->transport->init(&ws_config);
                }
                MA_LOGI(TAG, "camera websocket server started on port %d", ws_config.port);
            } else {
                instance->transport = nullptr;
            }
            
            models_.push_back(instance);
        }
        MA_CATCH(ma::Exception & e) {
            if (instance->engine != nullptr) {
                delete instance->engine;
                instance->engine = nullptr;
            }
            if (instance->model != nullptr) {
                delete instance->model;
                instance->model = nullptr;
            }
            if (instance->thread != nullptr) {
                delete instance->thread;
                instance->thread = nullptr;
            }
            if (instance->transport != nullptr) {
                delete instance->transport;
                instance->transport = nullptr;
            }
            delete instance;
            MA_THROW(e);
        }
        MA_CATCH(std::exception & e) {
            if (instance->engine != nullptr) {
                delete instance->engine;
                instance->engine = nullptr;
            }
            if (instance->model != nullptr) {
                delete instance->model;
                instance->model = nullptr;
            }
            if (instance->thread != nullptr) {
                delete instance->thread;
                instance->thread = nullptr;
            }
            if (instance->transport != nullptr) {
                delete instance->transport;
                instance->transport = nullptr;
            }
            delete instance;
            MA_THROW(Exception(MA_EINVAL, e.what()));
        }
    }

    created_ = true;

    server_->response(id_, json::object({{"type", MA_MSG_TYPE_RESP}, {"name", "create"}, {"code", MA_OK}, {"data", info_}}));

    return MA_OK;
}

ma_err_t ModelNode::onControl(const std::string& control, const json& data) {
    Guard guard(mutex_);
    ma_err_t err = MA_OK;
    if (control == "config") {
        if (data.contains("tscore") && data["tscore"].is_number_float()) {
            model_->setConfig(MA_MODEL_CFG_OPT_THRESHOLD, data["tscore"].get<float>());
        }
        if (data.contains("tiou") && data["tiou"].is_number_float()) {
            model_->setConfig(MA_MODEL_CFG_OPT_NMS, data["tiou"].get<float>());
        }
        if (data.contains("topk") && data["topk"].is_number_integer()) {
            model_->setConfig(MA_MODEL_CFG_OPT_TOPK, data["topk"].get<int32_t>());
        }
        if (data.contains("debug") && data["debug"].is_boolean()) {
            debug_ = data["debug"].get<bool>();
        }
        if (data.contains("trace") && data["trace"].is_boolean()) {
            trace_ = data["trace"].get<bool>();
            tracker_.clear();
        }
        if (data.contains("counting") && data["counting"].is_boolean()) {
            counting_ = data["counting"].get<bool>();
            counter_.clear();
        }
        if (data.contains("splitter") && data["splitter"].is_array()) {
            counter_.setSplitter(data["splitter"].get<std::vector<int16_t>>());
        }
        server_->response(id_, json::object({{"type", MA_MSG_TYPE_RESP}, {"name", control}, {"code", MA_OK}, {"data", data}}));
    } else if (control == "enabled" && data.is_boolean()) {
        bool enabled = data.get<bool>();
        if (enabled_.load() != enabled) {
            enabled_.store(enabled);
        }
        server_->response(id_, json::object({{"type", MA_MSG_TYPE_RESP}, {"name", control}, {"code", MA_OK}, {"data", enabled_.load()}}));
    } else {
        server_->response(id_, json::object({{"type", MA_MSG_TYPE_RESP}, {"name", control}, {"code", MA_ENOTSUP}, {"data", "Not supported"}}));
    }
    return MA_OK;
}

ma_err_t ModelNode::onDestroy() {
    Guard guard(mutex_);

    if (!created_) {
        return MA_OK;
    }

    onStop();

    if (thread_ != nullptr) {
        delete thread_;
        thread_ = nullptr;
    }
    if (engine_ != nullptr) {
        delete engine_;
        engine_ = nullptr;
    }
    if (model_ != nullptr) {
        delete model_;
        model_ = nullptr;
    }
    if (camera_ != nullptr) {
        camera_->detach(CHN_RAW, &raw_frame_);
        if (debug_) {
            camera_->detach(CHN_JPEG, &jpeg_frame_);
        }
    }
    if (transport_ != nullptr) {
        transport_->deInit();
        delete transport_;
        transport_ = nullptr;
    }


    created_ = false;

    return MA_OK;
}

/**
 * @brief 启动模型节点
 *
 * 该函数负责启动模型推理节点，包括：
 * 1. 查找并连接相机依赖节点
 * 2. 配置相机参数（分辨率、格式、帧率）
 * 3. 根据运行模式创建和管理线程
 *
 * ==================== 线程创建策略（按模式区分） ====================
 *
 * | 模式       | 线程数量 | 创建方式                                  | 执行方式                   |
 * |------------|----------|-------------------------------------------|--------------------------|
 * | parallel   | N        | 为每个模型实例创建独立线程                | 各模型独立运行，互不阻塞  |
 * | serial     | 1        | 单个串行执行线程                          | 依次执行所有模型          |
 *
 * ==================== 函数执行流程 ====================
 *
 *     ┌─────────────────────────────────────────────────────────────────┐
 *     │                      onStart()                                 │
 *     │                     无参数输入                                  │
 *     └─────────────────────────────────────────────────────────────────┘
 *                                    │
 *                                    ▼
 *     ┌─────────────────────────────────────────────────────────────────┐
 *     │  步骤1: 互斥锁保护                                              │
 *     │  ├─ Guard guard(mutex_)                                        │
 *     │  └─ 防止重复启动                                               │
 *     └─────────────────────────────────────────────────────────────────┘
 *                                    │
 *                                    ▼
 *     ┌─────────────────────────────────────────────────────────────────┐
 *     │  步骤2: 查找相机节点                                            │
 *     │  ├─ 遍历 dependencies_                                         │
 *     │  ├─ 查找 type == "camera" 的依赖节点                           │
 *     │  └─ 转换为 CameraNode*                                        │
 *     └─────────────────────────────────────────────────────────────────┘
 *                                    │
 *                                    ▼
 *     ┌─────────────────────────────────────────────────────────────────┐
 *     │  步骤3: 验证相机可用性                                          │
 *     │                                                                 │
 *     │  if (camera_ == nullptr)                                       │
 *     │      └─ 抛出异常: "No camera node found"                       │
 *     └─────────────────────────────────────────────────────────────────┘
 *                                    │
 *                                    ▼
 *     ┌─────────────────────────────────────────────────────────────────┐
 *     │  步骤4: 配置相机参数                                            │
 *     │                                                                 │
 *     │  获取第一个模型的输入参数:                                       │
 *     │  ├─ img->width  →  相机 RAW 通道宽度                           │
 *     │  ├─ img->height →  相机 RAW 通道高度                           │
 *     │  ├─ preview_fps →  相机帧率                                    │
 *     │  └─ img->format →  相机格式                                    │
 *     │                                                                 │
 *     │  配置 RAW 通道 (必须):                                          │
 *     │  └─ camera_->config(CHN_RAW, width, height, fps, format)       │
 *     │                                                                 │
 *     │  挂载 RAW 帧缓冲区:                                             │
 *     │  └─ camera_->attach(CHN_RAW, &raw_frame_)                      │
 *     │                                                                 │
 *     │  如果任一模型启用 debug 模式:                                   │
 *     │  ├─ 获取预览尺寸 (preview_width, preview_height)               │
 *     │  ├─ 配置 JPEG 通道:                                            │
 *     │  │  └─ camera_->config(CHN_JPEG, w, h, fps, JPEG)             │
 *     │  └─ 挂载 JPEG 帧缓冲区:                                        │
 *     │     └─ camera_->attach(CHN_JPEG, &jpeg_frame_)                 │
 *     └─────────────────────────────────────────────────────────────────┘
 *                                    │
 *                                    ▼
 *     ┌─────────────────────────────────────────────────────────────────┐
 *     │  步骤5: 设置运行状态                                            │
 *     │  └─ started_ = true                                            │
 *     └─────────────────────────────────────────────────────────────────┘
 *                                    │
 *                                    ▼
 *     ┌─────────────────────────────────────────────────────────────────┐
 *     │  步骤6: 根据模式创建线程                                        │
 *     │                                                                 │
 *     │  if (mode_ == "parallel")                                      │
 *     │      │                                                         │
 *     │      ├─ 并行模式流程:                                          │
 *     │      │  ┌─────────────────────────────────────────────────┐    │
 *     │      │  │ for each ModelInstance:                         │    │
 *     │      │  │   ├─ 设置 node, is_first, is_last               │    │
 *     │      │  │   ├─ 创建独立线程                                │    │
 *     │      │  │   │  └─ Thread(name, modelInstanceThreadEntry)  │    │
 *     │      │  │   └─ 启动线程                                    │    │
 *     │      │  │       └─ thread->start()                        │    │
 *     │      │  └─────────────────────────────────────────────────┘    │
 *     │      │                                                         │
 *     │      └─ [图示: 并行模式线程结构]                                │
 *     │              ┌─────────────────────────────────────┐           │
 *     │              │            主线程 onStart           │           │
 *     │              └──────────────────┬──────────────────┘           │
 *     │                         ┌───────┼───────┐                      │
 *     │                         │       │       │                      │
 *     │                         ▼       ▼       ▼                      │
 *     │              ┌─────────────┐  ┌─────────────┐  ┌────────────┐ │
 *     │              │ Thread#1    │  │ Thread#2    │  │ Thread#3   │ │
 *     │              │ Model1运行  │  │ Model2运行  │  │ Model3运行 │ │
 *     │              └─────────────┘  └─────────────┘  └────────────┘ │
 *     │                                                                 │
 *     │  else (serial)                                                 │
 *     │      │                                                         │
 *     │      ├─ 串行模式流程:                                          │
 *     │      │  ┌─────────────────────────────────────────────────┐    │
 *     │      │  │ for each ModelInstance:                         │    │
 *     │      │  │   ├─ 设置 node, is_first, is_last               │    │
 *     │      │  │   (不创建线程，仅标记)                           │    │
 *     │      │  └─────────────────────────────────────────────────┘    │
 *     │      │                                                         │
 *     │      │  ┌─────────────────────────────────────────────────┐    │
 *     │      │  │ 创建单一串行执行线程:                             │    │
 *     │      │  │   ├─ Thread(name, serialExecutionEntry)          │    │
 *     │      │  │   └─ 启动线程                                    │    │
 *     │      │  └──────────────────────────────────────────────── *     │          │      └─ [图示 │                                                         │
 *─┘    │
: 串行模式线程结构]                                │
 *     │              ┌─────────────────────────────────────┐           │
 *     │              │            主线程 onStart           │           │
 *     │              └──────────────────┬──────────────────┘           │
 *     │                                   │                            │
 *     │                                   ▼                            │
 *     │              ┌─────────────────────────────────────┐           │
 *     │              │     单一线程 (serialExecution)       │           │
 *     │              │     依次执行 Model1 → Model2 → Model3│           │
 *     │              └─────────────────────────────────────┘           │
 *     └─────────────────────────────────────────────────────────────────┘
 *                                    │
 *                                    ▼
 *     ┌─────────────────────────────────────────────────────────────────┐
 *     │  步骤7: 返回结果                                                │
 *     │  └─ return MA_OK                                               │
 *     └─────────────────────────────────────────────────────────────────┘
 *
 * ==================== 关键成员变量初始化 ====================
 *
 * | 变量名       | 初始化值  | 说明                                    |
 * |--------------|-----------|-----------------------------------------|
 * | camera_      | nullptr   | 指向依赖的相机节点                       |
 * | started_     | true      | 标记为运行状态                          |
 * | thread_      | nullptr   | 串行模式执行线程                        |
 * | instance->thread | nullptr | 每个模型的独立线程（并行模式）         |
 * | instance->is_first | -      | 标记是否为第一个模型                   |
 * | instance->is_last  | -      | 标记是否为最后一个模型                  |
 *
 * @return ma_err_t 错误码，MA_OK表示成功启动
 *
 * @note 并行模式下，每个模型的线程独立运行，互不等待
 * @note 串行模式下，所有模型在单一线程中按顺序执行
 * @note 如果没有找到相机节点，会抛出异常
 * @see onStop 停止模型节点
 * @see modelInstanceThreadEntry 并行模式线程入口
 * @see serialExecutionEntry 串行模式执行入口
 */
ma_err_t ModelNode::onStart() {
    Guard guard(mutex_);
    if (started_) {
        return MA_OK;
    }

    for (auto& dep : dependencies_) {
        if (dep.second->type() == "camera") {
            camera_ = static_cast<CameraNode*>(dep.second);
            break;
        }
    }

    if (camera_ == nullptr) {
        MA_THROW(Exception(MA_ENOTSUP, "No camera node found"));
        return MA_ENOTSUP;
    }

    // 配置相机 - 使用第一个模型的输入参数作为参考
    if (!models_.empty()) {
        const ma_img_t* img = static_cast<const ma_img_t*>(models_[0]->model->getInput());
        
        MA_LOGI(TAG, "onStart model: %s(%s) width %d height %d format %d", type_.c_str(), id_.c_str(), img->width, img->height, img->format);

        camera_->config(CHN_RAW, img->width, img->height, models_[0]->preview_fps, img->format);
        camera_->attach(CHN_RAW, &raw_frame_);
        
        // 检查是否需要启用debug模式
        bool any_debug = false;
        for (auto instance : models_) {
            if (instance->debug) {
                any_debug = true;
                break;
            }
        }
        
        if (any_debug) {
            int preview_width = models_[0]->preview_width;
            int preview_height = models_[0]->preview_height;
            
            if (preview_width == -1 || preview_height == -1) {
                preview_width  = img->width;
                preview_height = img->height;
            }
            
            camera_->config(CHN_JPEG, preview_width, preview_height, models_[0]->preview_fps, MA_PIXEL_FORMAT_JPEG);
            camera_->attach(CHN_JPEG, &jpeg_frame_);
        }
    }

    MA_LOGI(TAG, "start model: %s(%s)", type_.c_str(), id_.c_str());
    started_ = true;

    // 根据运行模式创建线程
    if (mode_ == "parallel") {
        // 并行模式：为每个模型实例创建独立线程
        for (size_t i = 0; i < models_.size(); ++i) {
            auto instance = models_[i];
            instance->node = this;
            instance->is_first = (i == 0);
            instance->is_last = (i == models_.size() - 1);
            if (instance->thread == nullptr) {
                instance->thread = new Thread((type_ + "#" + instance->id).c_str(), modelInstanceThreadEntryStub, instance);
            }
            instance->thread->start();
        }
    } else {
        // 串行模式（默认）：只创建一个线程，依次执行所有模型
        for (size_t i = 0; i < models_.size(); ++i) {
            auto instance = models_[i];
            instance->node = this;
            instance->is_first = (i == 0);
            instance->is_last = (i == models_.size() - 1);
        }
        if (thread_ == nullptr) {
            thread_ = new Thread((type_ + "#serial").c_str(), serialExecutionEntryStub, this);
        }
        thread_->start();
    }

    return MA_OK;
}

/**
 * @brief 停止模型节点
 *
 * 该函数负责停止模型推理节点，包括：
 * 1. 设置停止标志
 * 2. 等待并清理所有模型线程
 * 3. 释放输入/输出帧缓冲区
 * 4. 分离相机通道
 *
 * ==================== 资源清理流程 ====================
 *
 *     ┌─────────────────────────────────────────────────────────────────┐
 *     │                      onStop()                                  │
 *     │                     无参数输入                                  │
 *     └─────────────────────────────────────────────────────────────────┘
 *                                    │
 *                                    ▼
 *     ┌─────────────────────────────────────────────────────────────────┐
 *     │  步骤1: 互斥锁保护                                              │
 *     │  ├─ Guard guard(mutex_)                                        │
 *     │  └─ 防止重复停止                                               │
 *     └─────────────────────────────────────────────────────────────────┘
 *                                    │
 *                                    ▼
 *     ┌─────────────────────────────────────────────────────────────────┐
 *     │  步骤2: 设置停止标志                                            │
 *     │  └─ started_ = false                                           │
 *     └─────────────────────────────────────────────────────────────────┘
 *                                    │
 *                                    ▼
 *     ┌─────────────────────────────────────────────────────────────────┐
 *     │  步骤3: 唤醒并等待模型线程                                      │
 *     │                                                                 │
 *     │  for each ModelInstance:                                       │
 *     │  ┌───────────────────────────────────────────────────────────┐  │
 *     │  │ 3.1 唤醒被阻塞的线程                                       │  │
 *     │  │     ├─ instance->mutex.lock()                             │  │
 *     │  │     ├─ instance->data_ready = true                        │  │
 *     │  │     └─ instance->event.set(1)                             │  │
 *     │  │                                                             │  │
 *     │  │ 3.2 等待线程结束                                           │  │
 *     │  │     └─ instance->thread->join()                           │  │
 *     │  │                                                             │  │
 *     │  │ 3.3 释放帧缓冲区                                           │  │
 *     │  │     ├─ delete instance->input_frame                       │  │
 *     │  │     └─ delete instance->output_frame                      │  │
 *     │  └───────────────────────────────────────────────────────────┘  │
 *     └─────────────────────────────────────────────────────────────────┘
 *                                    │
 *                                    ▼
 *     ┌─────────────────────────────────────────────────────────────────┐
 *     │  步骤4: 清理串行模式线程                                        │
 *     │                                                                 │
 *     │  if (thread_ != nullptr)                                       │
 *     │      └─ thread_->join()                                        │
 *     │                                                                 │
 *     │  [图示: 线程清理流程]                                           │
 *     │  ┌─────────────────────────────────────────────────────────┐    │
 *     │  │  并行模式:              串行模式:                        │    │
 *     │  │                                                         │    │
 *     │  │  Thread#1 ──────┐        Thread#serial ────────────┐    │    │
 *     │  │        join()   │               join()              │    │    │
 *     │  │  Thread#2 ──────┤                                  │    │    │
 *     │  │        join()   │                                  │    │    │
 *     │  │  Thread#3 ──────┘                                  │    │    │
 *     │  │                              ↓                     │    │    │
 *     │  │              所有线程完成后继续执行                 │    │    │
 *     │  └─────────────────────────────────────────────────────────┘    │
 *     └─────────────────────────────────────────────────────────────────┘
 *                                    │
 *                                    ▼
 *     ┌─────────────────────────────────────────────────────────────────┐
 *     │  步骤5: 分离相机通道                                           │
 *     │                                                                 │
 *     │  if (camera_ != nullptr)                                       │
 *     │  ┌───────────────────────────────────────────────────────────┐  │
 *     │  │ 5.1 分离 RAW 通道                                         │  │
 *     │  │     └─ camera_->detach(CHN_RAW, &raw_frame_)             │  │
 *     │  │                                                           │  │
 *     │  │ 5.2 分离 JPEG 通道 (仅当 debug 模式启用时)                │  │
 *     │  │     └─ camera_->detach(CHN_JPEG, &jpeg_frame_)           │  │
 *     │  │                                                           │  │
 *     │  │ 5.3 清理相机指针                                          │  │
 *     │  │     └─ camera_ = nullptr                                 │  │
 *     │  └───────────────────────────────────────────────────────────┘  │
 *     │                                                                 │
 *     │  [图示: 相机通道分离]                                           │
 *     │  ┌─────────────────────────────────────────────────────────┐    │
 *     │  │           相机 (camera_)                                │    │
 *     │  │    ┌──────────────┬──────────────┐                     │    │
 *     │  │    │   CHN_RAW    │   CHN_JPEG   │                     │    │
 *     │  │    │   (必须)     │  (可选)      │                     │    │
 *     │  │    └──────┬───────┴──────┬───────┘                     │    │
 *     │  │           │              │                             │    │
 *     │  │           ↓              ↓                             │    │
 *     │  │      detach()      detach()                            │    │
 *     │  │           │              │                             │    │
 *     │  │           ↓              ↓                             │    │
 *     │  │      raw_frame_    jpeg_frame_                         │    │
 *     │  └─────────────────────────────────────────────────────────┘    │
 *     └─────────────────────────────────────────────────────────────────┘
 *                                    │
 *                                    ▼
 *     ┌─────────────────────────────────────────────────────────────────┐
 *     │  步骤6: 返回结果                                                │
 *     │  └─ return MA_OK                                               │
 *     └─────────────────────────────────────────────────────────────────┘
 *
 * ==================== 资源清理对照表 ====================
 *
 * | 资源类型       | 清理方式                    | 条件说明                       |
 * |----------------|-----------------------------|--------------------------------|
 * | 模型线程       | thread->join()              | 无论并行/串行模式都清理        |
 * | 输入帧缓冲区   | delete input_frame          | 仅在串行模式下分配             |
 * | 输出帧缓冲区   | delete output_frame         | 仅在串行模式下分配             |
 * | 串行执行线程   | thread_->join()             | 仅在串行模式下创建             |
 * | RAW 通道       | camera_->detach(CHN_RAW)    | 总是执行                       |
 * | JPEG 通道      | camera_->detach(CHN_JPEG)   | 仅当 debug 模式启用时          |
 * | 相机指针       | camera_ = nullptr           | 清理引用                       |
 *
 * ==================== 线程同步说明 ====================
 *
 * | 场景           | 同步机制                    | 说明                           |
 * |----------------|-----------------------------|--------------------------------|
 * | 停止线程       | event.set(1)                | 唤醒被 event.wait() 阻塞的线程 |
 * | 等待线程结束   | thread->join()              | 阻塞主线程直到子线程结束       |
 * | 防止重入       | Guard guard(mutex_)         | 防止 onStart/onStop 并发调用   |
 *
 * @return ma_err_t 错误码，MA_OK表示成功停止
 *
 * @note 该函数会阻塞直到所有线程完成
 * @note 线程唤醒使用 data_ready + event 机制
 * @note 输入/输出帧仅在串行模式下由代码显式分配
 * @see onStart 启动模型节点
 * @see modelInstanceThreadEntry 并行模式线程入口
 * @see serialExecutionEntry 串行模式执行入口
 */
ma_err_t ModelNode::onStop() {
    Guard guard(mutex_);
    if (!started_) {
        return MA_OK;
    }
    started_ = false;

    // 停止并等待所有模型线程
    for (auto instance : models_) {
        if (instance->thread != nullptr) {
            instance->mutex.lock();
            instance->data_ready = true;
            instance->event.set(1);
            instance->mutex.unlock();
            instance->thread->join();
        }
        if (instance->input_frame != nullptr) {
            delete instance->input_frame;
            instance->input_frame = nullptr;
        }
        if (instance->output_frame != nullptr) {
            delete instance->output_frame;
            instance->output_frame = nullptr;
        }
    }

    // 停止串行模式线程
    if (thread_ != nullptr) {
        thread_->join();
    }

    if (camera_ != nullptr) {
        camera_->detach(CHN_RAW, &raw_frame_);
        if (debug_) {
            camera_->detach(CHN_JPEG, &jpeg_frame_);
        }
        camera_ = nullptr;
    }
    return MA_OK;
}

REGISTER_NODE_SINGLETON("model", ModelNode);

/**
 * @brief 输入图像预处理函数
 *
 * 对输入图像进行预处理，包括颜色空间转换、尺寸调整、归一化等操作，
 * 使其符合模型输入要求。该函数用于单模型模式的预处理流程。
 *
 * ==================== 预处理流程图 ====================
 *
 *     ┌─────────────────────────────────────────────────────────────────┐
 *     │                     preprocessImage()                          │
 *     │                       输入: cv::Mat                            │
 *     └─────────────────────────────────────────────────────────────────┘
 *                                    │
 *                                    ▼
 *     ┌─────────────────────────────────────────────────────────────────┐
 *     │  步骤1: 颜色空间转换                                             │
 *     │                                                                 │
 *     │  input_format = "GRAY"  →  BGR → GRAY                          │
 *     │  input_format = "RGB"   →  BGR → RGB                           │
 *     │  其他              →  保持 BGR                                  │
 *     └─────────────────────────────────────────────────────────────────┘
 *                                    │
 *                                    ▼
 *     ┌─────────────────────────────────────────────────────────────────┐
 *     │  步骤2: R/B通道交换（可选）                                      │
 *     │                                                                 │
 *     │  swap_rb = true  →  RGB → BGR                                  │
 *     │  swap_rb = false →  保持原样                                    │
 *     └─────────────────────────────────────────────────────────────────┘
 *                                    │
 *                                    ▼
 *     ┌─────────────────────────────────────────────────────────────────┐
 *     │  步骤3: 尺寸调整                                                 │
 *     │                                                                 │
 *     │  letterbox = true:                                             │
 *     │      ├─ 计算保持宽高比的缩放比例                                 │
 *     │      ├─ 缩放到目标尺寸内最大尺寸                                 │
 *     │      └─ 使用114填充边界 (上下左右对称)                           │
 *     │                                                                 │
 *     │  letterbox = false:                                            │
 *     │      └─ 直接缩放到目标尺寸 (可能变形)                            │
 *     └─────────────────────────────────────────────────────────────────┘
 *                                    │
 *                                    ▼
 *     ┌─────────────────────────────────────────────────────────────────┐
 *     │  步骤4: 数据类型转换                                             │
 *     │                                                                 │
 *     │  CV_8UC3 → CV_32FC3 (归一化到 [0,1])                            │
 *     └─────────────────────────────────────────────────────────────────┘
 *                                    │
 *                                    ▼
 *     ┌─────────────────────────────────────────────────────────────────┐
 *     │  步骤5: 归一化处理（可选）                                       │
 *     │                                                                 │
 *     │  output = (output - mean) / std                                │
 *     │                                                                 │
 *     │  通常用于与预训练模型的输入匹配:                                 │
 *     │  - mean = [0.485, 0.456, 0.406] (ImageNet均值)                 │
 *     │  - std  = [0.229, 0.224, 0.225] (ImageNet标准差)                │
 *     └─────────────────────────────────────────────────────────────────┘
 *                                    │
 *                                    ▼
 *     ┌─────────────────────────────────────────────────────────────────┐
 *     │                      输出: cv::Mat (CV_32FC3)                   │
 *     └─────────────────────────────────────────────────────────────────┘
 *
 * ==================== Letterbox vs 拉伸缩放对比 ====================
 *
 *     ┌──────────────────────┬──────────────────────────────────────────┐
 *     │      Letterbox       │            拉伸缩放                       │
 *     ├──────────────────────┼──────────────────────────────────────────┤
 *     │                      │                                          │
 *     │  ┌──────────────┐   │    ┌──────────────────────────────┐       │
 *     │  │              │   │    │                              │       │
 *     │  │   原始图像   │   │    │                              │       │
 *     │  │              │   │    │       原始图像               │       │
 *     │  │              │   │    │                              │       │
 *     │  └──────────────┘   │    │                              │       │
 *     │        ↓            │    └──────────────────────────────┘       │
 *     │  ┌──────────────────┤             ↓                           │
 *     │  │  ┌────────────┐ │    ┌──────────────────────────────┐       │
 *     │  │  │            │ │    │                              │       │
 *     │  │  │  缩放后    │ │    │       拉伸后 (变形)           │       │
 *     │  │  │  图像      │ │    │                              │       │
 *     │  │  │            │ │    │                              │       │
 *     │  │  └────────────┘ │    │                              │       │
 *     │  │  114 114 114    │    └──────────────────────────────┘       │
 *     │  └──────────────────┤                                          │
 *     │      保持宽高比     │       可能变形，填充信息丢失               │
 *     └──────────────────────┴──────────────────────────────────────────┘
 *
 * @param input  输入图像，BGR格式的cv::Mat
 * @param config 预处理配置参数，包含输入尺寸、格式、归一化等设置
 * @return cv2::Mat 预处理后的图像，CV_32FC3格式
 *
 * @note 输入图像通常来自摄像头，格式为BGR
 * @note 输出图像格式为CV_32FC3 (32位浮点，3通道)
 * @note 归一化参数需要与模型训练时的预处理保持一致
 * @see PreprocessConfig
 * @see convertResultToInput 用于多模型模式的结果转换
 */
cv2::Mat ModelNode::preprocessImage(const ::cv::Mat& input, const PreprocessConfig& config) {
    cv2::Mat output;
    
    if (config.input_format == "GRAY") {
        cv2::cvtColor(input, output, cv2::COLOR_BGR2GRAY);
    } else if (config.input_format == "RGB") {
        cv2::cvtColor(input, output, cv2::COLOR_BGR2RGB);
    } else {
        output = input;
    }
    
    if (config.swap_rb) {
        cv2::cvtColor(output, output, cv2::COLOR_RGB2BGR);
    }
    
    if (config.letterbox) {
        int target_width = config.input_width;
        int target_height = config.input_height;
        float scale = std::min(static_cast<float>(target_width) / input.cols, 
                              static_cast<float>(target_height) / input.rows);
        int new_width = static_cast<int>(input.cols * scale);
        int new_height = static_cast<int>(input.rows * scale);
        
        cv2::resize(output, output, cv2::Size(new_width, new_height));
        
        int top = (target_height - new_height) / 2;
        int bottom = target_height - new_height - top;
        int left = (target_width - new_width) / 2;
        int right = target_width - new_width - left;
        
        cv2::copyMakeBorder(output, output, top, bottom, left, right, 
                            cv2::BORDER_CONSTANT, cv2::Scalar(114, 114, 114));
    } else {
        cv2::resize(output, output, cv2::Size(config.input_width, config.input_height));
    }
    
    output.convertTo(output, CV_32F);
    
    if (config.normalize) {
        if (config.mean.size() == 3 && config.std.size() == 3) {
            cv2::subtract(output, cv2::Scalar(config.mean[0], config.mean[1], config.mean[2]), output);
            cv2::divide(output, cv2::Scalar(config.std[0], config.std[1], config.std[2]), output);
        }
    }
    
    return output;
}

/**
 * @brief 模型输出结果转换为下一模型输入函数
 *
 * 该函数用于串行执行模式下，将上一模型的输出结果转换为下一模型的输入格式。
 * 它类似于 preprocessImage，但在多模型级联场景中有特殊用途：
 * - 从原始帧中提取感兴趣区域 (ROI)
 * - 应用与 preprocessImage 相同的预处理流程
 * - 为下一阶段的模型推理准备输入数据
 *
 * ==================== 与 preprocessImage 的对比 ====================
 *
 * | 特性         | preprocessImage              | convertResultToInput            |
 * |--------------|------------------------------|---------------------------------|
 * | 输入来源     | 摄像头原始帧                  | 上一模型的输出结果              |
 * | ROI提取      | 不需要                       | 需要（从原始帧裁剪）            |
 * | 目标尺寸     | config.input_width/height    | 模型实际输入尺寸 (config优先)   |
 * | 使用场景     | 单模型模式 / 第一模型         | 串行模式的后续模型              |
 * | 输入格式     | BGR (摄像头)                 | 任意 Mat                        |
 *
 * ==================== 处理流程图 ====================
 *
 *     ┌─────────────────────────────────────────────────────────────────┐
 *     │                  convertResultToInput()                        │
 *     │  输入: original_frame (来自上一模型输出)                         │
 *     └─────────────────────────────────────────────────────────────────┘
 *                                    │
 *                                    ▼
 *     ┌─────────────────────────────────────────────────────────────────┐
 *     │  参数验证                                                       │
 *     │  ├─ model != nullptr?                                          │
 *     │  ├─ original_frame.empty()?                                    │
 *     │  └─ 获取模型实际输入尺寸 (优先使用config)                        │
 *     └─────────────────────────────────────────────────────────────────┘
 *                                    │
 *                                    ▼
 *     ┌─────────────────────────────────────────────────────────────────┐
 *     │  步骤1: 尺寸调整（Letterbox 或 拉伸）                            │
 *     │                                                                 │
 *     │  letterbox = true:                                             │
 *     │      ├─ 计算保持宽高比的缩放比例                                 │
 *     │      ├─ 缩放到目标尺寸内最大尺寸                                 │
 *     │      └─ 使用114填充边界                                         │
 *     │                                                                 │
 *     │  letterbox = false:                                            │
 *     │      └─ 直接拉伸到目标尺寸                                      │
 *     └─────────────────────────────────────────────────────────────────┘
 *                                    │
 *                                    ▼
 *     ┌─────────────────────────────────────────────────────────────────┐
 *     │  步骤2: 颜色空间转换                                             │
 *     │                                                                 │
 *     │  input_format = "RGB"  →  BGR → RGB                            │
 *     │  input_format = "GRAY" →  BGR → GRAY → BGR                     │
 *     │  swap_rb = true      →  RGB → BGR                              │
 *     └─────────────────────────────────────────────────────────────────┘
 *                                    │
 *                                    ▼
 *     ┌─────────────────────────────────────────────────────────────────┐
 *     │  步骤3: 数据类型转换与归一化                                     │
 *     │                                                                 │
 *     │  CV_8UC3 → CV_32FC3                                            │
 *     │  output = (output - mean) / std  (如果启用normalize)            │
 *     └─────────────────────────────────────────────────────────────────┘
 *                                    │
 *                                    ▼
 *     ┌─────────────────────────────────────────────────────────────────┐
 *     │                     输出: cv::Mat (CV_32FC3)                    │
 *     │                   准备送入下一模型推理                           │
 *     └─────────────────────────────────────────────────────────────────┘
 *
 * ==================== 串行模式中的数据流转 ====================
 *
 *   Model 1 输出                    convertResultToInput              Model 2 输入
 *        │                                    │                              │
 *        │  检测结果 (bbox)                   │                              │
 *        │        ↓                           │                              │
 *        │  裁剪ROI区域                       │                              │
 *        │        ↓                           │                              │
 *        │  ────────────────────────────────> │                              │
 *        │           original_frame          │                              │
 *        │                                    ↓                              │
 *        │                           ┌─────────────────┐                   │
 *        │                           │                 │                   │
 *        │                           │  Letterbox调整  │                   │
 *        │                           │  颜色空间转换   │                   │
 *        │                           │  归一化处理     │                   │
 *        │                           │                 │                   │
 *        │                           └─────────────────┘                   │
 *        │                                    ↓                              │
 *        │                           ┌─────────────────┐                   │
 *        │                           │   CV_32FC3     │                   │
 *        │                           │   预处理完成    │ ──────────────────┼──> tensor
 *        │                           └─────────────────┘                   │
 *
 * @param original_frame 上一模型的输出结果图像，可以是任意格式的cv::Mat
 * @param model 目标模型指针，用于获取模型的输入尺寸要求
 * @param config 预处理配置参数，包含输入尺寸、格式、归一化等设置
 * @return cv2::Mat 预处理后的图像，CV_32FC3格式，可直接用于模型推理
 *
 * @note 在串行模式下，此函数用于处理非第一个模型的输入
 * @note 如果 config.input_width/height 为0，则使用模型的默认输入尺寸
 * @note ROI提取应该在调用此函数之前完成，此函数只负责预处理
 * @see preprocessImage 用于第一个模型或单模型模式的预处理
 * @see serialExecutionEntry 串行执行模式的主函数
 */
cv2::Mat ModelNode::convertResultToInput(const ::cv::Mat& original_frame, Model* model, const PreprocessConfig& config) {
    cv2::Mat roi;
    
    if (model == nullptr || original_frame.empty()) {
        return roi;
    }
    
    const ma_img_t* input = static_cast<const ma_img_t*>(model->getInput());
    if (input == nullptr) {
        return roi;
    }
    
    int model_input_width = (config.input_width > 0) ? config.input_width : input->width;
    int model_input_height = (config.input_height > 0) ? config.input_height : input->height;
    
    cv2::Mat resized;
    cv2::Size target_size(model_input_width, model_input_height);
    
    if (config.letterbox) {
        float scale = std::min(static_cast<float>(target_size.width) / original_frame.cols,
                              static_cast<float>(target_size.height) / original_frame.rows);
        int new_width = static_cast<int>(original_frame.cols * scale);
        int new_height = static_cast<int>(original_frame.rows * scale);
        
        cv2::resize(original_frame, resized, cv2::Size(new_width, new_height));
        
        int top = (target_size.height - new_height) / 2;
        int bottom = target_size.height - new_height - top;
        int left = (target_size.width - new_width) / 2;
        int right = target_size.width - new_width - left;
        
        cv2::copyMakeBorder(resized, roi, top, bottom, left, right,
                          cv2::BORDER_CONSTANT, cv2::Scalar(114, 114, 114));
    } else {
        cv2::resize(original_frame, roi, target_size);
    }
    
    if (config.input_format == "RGB") {
        cv2::cvtColor(roi, roi, cv2::COLOR_BGR2RGB);
    } else if (config.input_format == "GRAY") {
        cv2::cvtColor(roi, roi, cv2::COLOR_BGR2GRAY);
        if (roi.channels() == 1) {
            cv2::cvtColor(roi, roi, cv2::COLOR_GRAY2BGR);
        }
    }
    
    if (config.swap_rb) {
        cv2::cvtColor(roi, roi, cv2::COLOR_RGB2BGR);
    }
    
    roi.convertTo(roi, CV_32F);
    
    if (config.normalize && config.mean.size() == 3 && config.std.size() == 3) {
        cv2::subtract(roi, cv2::Scalar(config.mean[0], config.mean[1], config.mean[2]), roi);
        cv2::divide(roi, cv2::Scalar(config.std[0], config.std[1], config.std[2]), roi);
    }
    
    return roi;
}

}  // namespace ma::node