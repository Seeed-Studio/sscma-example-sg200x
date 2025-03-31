#include <unistd.h>

#include <opencv2/opencv.hpp>
namespace cv2 = cv;

#include "model.h"

namespace ma::node {

using namespace ma::engine;
using namespace ma::model;

static constexpr char TAG[] = "ma::node::model";

#define DEFAULT_MODEL "/userdata/MODEL/model.cvimodel"

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
      camera_(nullptr) {}

ModelNode::~ModelNode() {
    onDestroy();
}
void ModelNode::threadEntry() {

    ma_err_t err     = MA_OK;
    videoFrame* raw  = nullptr;
    videoFrame* jpeg = nullptr;
    int32_t width    = 0;
    int32_t height   = 0;
    std::vector<std::string> labels;

    server_->response(id_, json::object({{"type", MA_MSG_TYPE_RESP}, {"name", "enabled"}, {"code", MA_OK}, {"data", enabled_.load()}}));

    while (started_) {

        if (!raw_frame_.fetch(reinterpret_cast<void**>(&raw), Tick::fromSeconds(2))) {
            continue;
        }
        if (debug_ && !jpeg_frame_.fetch(reinterpret_cast<void**>(&jpeg), Tick::fromSeconds(2))) {
            raw->release();
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

        json reply = json::object({{"type", MA_MSG_TYPE_EVT}, {"name", "invoke"}, {"code", MA_OK}, {"data", {{"count", ++count_}}}});

        if (debug_) {
            width  = jpeg->img.width;
            height = jpeg->img.height;
        } else {
            width  = raw->img.width;
            height = raw->img.height;
        }


        reply["data"]["resolution"] = json::array({width, height});

        ma_tensor_t tensor = {
            .is_physical = true,
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
                    reply["data"]["boxes"].push_back({static_cast<int16_t>(_bboxes[i].x * width),
                                                      static_cast<int16_t>(_bboxes[i].y * height),
                                                      static_cast<int16_t>(_bboxes[i].w * width),
                                                      static_cast<int16_t>(_bboxes[i].h * height),
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
                    reply["data"]["boxes"].push_back({static_cast<int16_t>(_bboxes[i].x * width),
                                                      static_cast<int16_t>(_bboxes[i].y * height),
                                                      static_cast<int16_t>(_bboxes[i].w * width),
                                                      static_cast<int16_t>(_bboxes[i].h * height),
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
                    pts.push_back({static_cast<int16_t>(pt.x * width), static_cast<int16_t>(pt.y * height), static_cast<int8_t>(pt.z * 100)});
                }
                json box = {static_cast<int16_t>(result.box.x * width),
                            static_cast<int16_t>(result.box.y * height),
                            static_cast<int16_t>(result.box.w * width),
                            static_cast<int16_t>(result.box.h * height),
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
                json box = {static_cast<int16_t>(result.box.x * width),
                            static_cast<int16_t>(result.box.y * height),
                            static_cast<int16_t>(result.box.w * width),
                            static_cast<int16_t>(result.box.h * height),
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
                    float w_scale = width / result.mask.width;
                    float h_scale = height / result.mask.height;
                    for (auto& c : *maxContour) {
                        contour.push_back(static_cast<uint16_t>(c.x * w_scale));
                        contour.push_back(static_cast<uint16_t>(c.y * h_scale));
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

ma_err_t ModelNode::onCreate(const json& config) {
    ma_err_t err = MA_OK;
    Guard guard(mutex_);

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

    MA_TRY {
        engine_ = new EngineDefault();

        if (engine_ == nullptr) {
            MA_THROW(Exception(MA_ENOMEM, "Engine init failed"));
        }
        if (engine_->init() != MA_OK) {
            MA_THROW(Exception(MA_EINVAL, "Engine init failed"));
        }
        if (engine_->load(uri_) != MA_OK) {
            MA_THROW(Exception(MA_EINVAL, "Engine load failed"));
        }
        model_ = ModelFactory::create(engine_, algorithm_);
        if (model_ == nullptr) {
            MA_THROW(Exception(MA_ENOTSUP, "Model not supported"));
        }

        MA_LOGI(TAG, "model: %s %s", uri_.c_str(), model_->getName());
        {  // extra config
            if (config.contains("tscore")) {
                model_->setConfig(MA_MODEL_CFG_OPT_THRESHOLD, config["tscore"].get<float>());
            }
            if (config.contains("tiou")) {
                model_->setConfig(MA_MODEL_CFG_OPT_NMS, config["tiou"].get<float>());
            }
            if (config.contains("topk")) {
                model_->setConfig(MA_MODEL_CFG_OPT_TOPK, config["tiou"].get<float>());
            }
            if (config.contains("debug")) {
                output_ = config["debug"].get<bool>();
            }
            if (config.contains("websocket") && config["websocket"].is_boolean()) {
                websocket_ = config["websocket"].get<bool>();
            }
            if (websocket_ || output_) {
                debug_ = true;
            }
            if (config.contains("trace")) {
                trace_ = config["trace"].get<bool>();
            }
            if (config.contains("counting")) {
                counting_ = config["counting"].get<bool>();
            }
            if (config.contains("splitter") && config["splitter"].is_array()) {
                counter_.setSplitter(config["splitter"].get<std::vector<int16_t>>());
            }
        }

        if (websocket_) {
            TransportWebSocket::Config ws_config = {.port = 8090};
            MA_STORAGE_GET_POD(server_->getStorage(), MA_STORAGE_KEY_WS_PORT, ws_config.port, 8090);
            transport_ = new TransportWebSocket();
            if (transport_ != nullptr) {
                transport_->init(&ws_config);
            }
            MA_LOGI(TAG, "camera websocket server started on port %d", ws_config.port);
        } else {
            transport_ = nullptr;
        }

        thread_ = new Thread((type_ + "#" + id_).c_str(), &ModelNode::threadEntryStub, this);
        if (thread_ == nullptr) {
            MA_THROW(Exception(MA_ENOMEM, "Not enough memory"));
        }
    }
    MA_CATCH(ma::Exception & e) {
        if (engine_ != nullptr) {
            delete engine_;
            engine_ = nullptr;
        }
        if (model_ != nullptr) {
            delete model_;
            model_ = nullptr;
        }
        if (thread_ != nullptr) {
            delete thread_;
            thread_ = nullptr;
        }
        MA_THROW(e);
    }
    MA_CATCH(std::exception & e) {
        if (engine_ != nullptr) {
            delete engine_;
            engine_ = nullptr;
        }
        if (model_ != nullptr) {
            delete model_;
            model_ = nullptr;
        }
        if (thread_ != nullptr) {
            delete thread_;
            thread_ = nullptr;
        }
        MA_THROW(Exception(MA_EINVAL, e.what()));
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

ma_err_t ModelNode::onStart() {
    Guard guard(mutex_);
    if (started_) {
        return MA_OK;
    }

    const ma_img_t* img = static_cast<const ma_img_t*>(model_->getInput());

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

    camera_->config(CHN_RAW, img->width, img->height, 30, img->format);
    camera_->attach(CHN_RAW, &raw_frame_);
    if (debug_) {
        camera_->config(CHN_JPEG, img->width, img->height, 30, MA_PIXEL_FORMAT_JPEG);
        camera_->attach(CHN_JPEG, &jpeg_frame_);
    }

    MA_LOGI(TAG, "start model: %s(%s)", type_.c_str(), id_.c_str());
    started_ = true;

    thread_->start(this);

    return MA_OK;
}

ma_err_t ModelNode::onStop() {
    Guard guard(mutex_);
    if (!started_) {
        return MA_OK;
    }
    started_ = false;

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

}  // namespace ma::node