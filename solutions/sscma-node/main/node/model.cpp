#include <unistd.h>

#include "model.h"

namespace ma::node {

using namespace ma::engine;
using namespace ma::model;

static constexpr char TAG[] = "ma::node::model";

#define DEFAULT_MODEL "/userdata/MODEL/model.cvimodel"

ModelNode::ModelNode(std::string id)
    : Node("model", id), uri_(""), debug_(true), trace_(false), counting_(false), count_(0), engine_(nullptr), model_(nullptr), thread_(nullptr), raw_frame_(1), jpeg_frame_(1) {}

ModelNode::~ModelNode() {}


void ModelNode::threadEntry() {

    ma_err_t err           = MA_OK;
    Detector* detector     = nullptr;
    Classifier* classifier = nullptr;
    videoFrame* raw        = nullptr;
    videoFrame* jpeg       = nullptr;
    int32_t width          = 0;
    int32_t height         = 0;
    ma_tick_t take         = 0;

    switch (model_->getType()) {
        case MA_MODEL_TYPE_IMCLS:
            classifier = static_cast<Classifier*>(model_);
            break;
        default:
            detector = static_cast<Detector*>(model_);
            break;
    }

    while (true) {
        if (!raw_frame_.fetch(reinterpret_cast<void**>(&raw))) {
            continue;
        }
        if (debug_ && !jpeg_frame_.fetch(reinterpret_cast<void**>(&jpeg))) {
            raw->release();
            continue;
        }
        Thread::enterCritical();
        json reply = json::object({{"type", MA_MSG_TYPE_EVT}, {"name", "invoke"}, {"code", MA_OK}, {"data", {{"count", ++count_}}}});

        width  = raw->img.width;
        height = raw->img.height;

        reply["data"]["resolution"] = json::array({width, height});

        ma_tensor_t tensor = {
            .is_physical = true,
            .is_variable = false,
        };
        tensor.data.data = reinterpret_cast<void*>(raw->phy_addr);
        engine_->setInput(0, tensor);
        model_->setRunDone([this, raw](void* ctx) { raw->release(); });
        if (detector != nullptr) {
            err                    = detector->run(nullptr);
            auto _perf             = detector->getPerf();
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
                }
            }
            if (counting_) {
                reply["data"]["counts"] = counter_.get();
                reply["data"]["lines"]  = counter_.getSplitter();
            }

            reply["data"]["perf"].push_back({_perf.preprocess, _perf.inference, _perf.postprocess});
        } else if (classifier != nullptr) {
            err           = classifier->run(nullptr);
            auto _perf    = classifier->getPerf();
            auto _results = classifier->getResults();
            reply["data"]["perf"].push_back({_perf.preprocess, _perf.inference, _perf.postprocess});
            reply["data"]["classes"] = json::array();
            for (auto& result : _results) {
                reply["data"]["classes"].push_back({static_cast<int8_t>(result.score * 100), result.target});
            }
        }

        if (classes_.size() > 0) {
            reply["data"]["labels"] = classes_;
        }

        if (debug_) {
            int base64_len = 4 * ((jpeg->img.size + 2) / 3 + 2);
            char* base64   = new char[base64_len];
            if (base64 != nullptr) {
                memset(base64, 0, base64_len);
                ma::utils::base64_encode(jpeg->img.data, jpeg->img.size, base64, &base64_len);
                reply["data"]["image"] = std::string(base64, base64_len);
                jpeg->release();
                delete[] base64;
            }
        } else {
            reply["data"]["image"] = "";
        }

        server_->response(id_, reply);

        Thread::exitCritical();
    }
}

void ModelNode::threadEntryStub(void* obj) {
    reinterpret_cast<ModelNode*>(obj)->threadEntry();
}

ma_err_t ModelNode::onCreate(const json& config) {
    ma_err_t err = MA_OK;
    Guard guard(mutex_);

    classes_.clear();

    if (config.contains("uri") && config["uri"].is_string()) {
        uri_ = config["uri"].get<std::string>();
    }

    if (uri_.empty()) {
        uri_ = DEFAULT_MODEL;
    }

    if (access(uri_.c_str(), R_OK) != 0) {
        MA_THROW(Exception(MA_ENOENT, "model file not found: " + uri_));
    }

    // find model.json
    size_t pos = uri_.find_last_of(".");
    if (pos != std::string::npos) {
        std::string path = uri_.substr(0, pos) + ".json";
        if (access(path.c_str(), R_OK) == 0) {
            std::ifstream ifs(path);
            if (!ifs.is_open()) {
                MA_THROW(Exception(MA_EINVAL, "model json not found: " + path));
            }
            ifs >> info_;
            if (info_.is_object()) {
                if (info_.contains("classes") && info_["classes"].is_array()) {
                    classes_ = info_["classes"].get<std::vector<std::string>>();
                }
            }
        }
    }

    // override classes
    if (classes_.size() == 0 && config.contains("classes") && config["classes"].is_array()) {
        classes_ = config["classes"].get<std::vector<std::string>>();
    }

    MA_TRY {
        engine_ = new EngineDefault();

        if (engine_ == nullptr) {
            MA_THROW(Exception(MA_ENOMEM, "Engine create failed"));
        }
        if (engine_->init() != MA_OK) {
            MA_THROW(Exception(MA_EINVAL, "Engine init failed"));
        }
        if (engine_->load(uri_) != MA_OK) {
            MA_THROW(Exception(MA_EINVAL, "Engine load failed"));
        }
        model_ = ModelFactory::create(engine_);
        if (model_ == nullptr) {
            MA_THROW(Exception(MA_ENOTSUP, "Model Not Supported"));
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
                debug_ = config["debug"].get<bool>();
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

        thread_ = new Thread((type_ + "#" + id_).c_str(), &ModelNode::threadEntryStub, this);
        if (thread_ == nullptr) {
            MA_THROW(Exception(MA_ENOMEM, "Thread create failed"));
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
            model_->setConfig(MA_MODEL_CFG_OPT_TOPK, data["tiou"].get<int32_t>());
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
    } else {
        server_->response(id_, json::object({{"type", MA_MSG_TYPE_RESP}, {"name", control}, {"code", MA_ENOTSUP}, {"data", ""}}));
    }
    return MA_OK;
}

ma_err_t ModelNode::onDestroy() {
    Guard guard(mutex_);
    this->onStop();
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
    server_->response(id_, json::object({{"type", MA_MSG_TYPE_RESP}, {"name", "destroy"}, {"code", MA_OK}, {"data", ""}}));
    return MA_OK;
}

ma_err_t ModelNode::onStart() {
    Guard guard(mutex_);
    if (started_) {
        return MA_OK;
    }

    const ma_img_t* img = nullptr;

    for (auto& dep : dependencies_) {
        if (dep.second->type() == "camera") {
            camera_ = static_cast<CameraNode*>(dep.second);
            break;
        }
    }

    switch (model_->getType()) {
        case MA_MODEL_TYPE_IMCLS:
            img = static_cast<Classifier*>(model_)->getInputImg();
        default:
            img = static_cast<Detector*>(model_)->getInputImg();
    }

    camera_->config(CHN_RAW, img->width, img->height, 30, img->format);
    camera_->attach(CHN_RAW, &raw_frame_);
    if (debug_) {
        camera_->config(CHN_JPEG, img->width, img->height, 30, MA_PIXEL_FORMAT_JPEG);
        camera_->attach(CHN_JPEG, &jpeg_frame_);
    }


    MA_LOGI(TAG, "start model: %s(%s)", type_.c_str(), id_.c_str());
    thread_->start(this);

    started_ = true;
    return MA_OK;
}

ma_err_t ModelNode::onStop() {
    Guard guard(mutex_);
    if (!started_) {
        return MA_OK;
    }
    started_ = false;
    if (thread_ != nullptr) {
        thread_->stop();
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