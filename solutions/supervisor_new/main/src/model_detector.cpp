#include <chrono>
#include <fstream>
#include <iostream>
#include <numeric>
#include <stdio.h>
#include <string>
#include <opencv2/opencv.hpp>
#include <sscma.h>


class ColorPalette {
public:
    static std::vector<cv::Scalar> getPalette() {
        return palette;
    }

    static cv::Scalar getColor(int index) {
        return palette[index % palette.size()];
    }

private:
    static const std::vector<cv::Scalar> palette;
};

const std::vector<cv::Scalar> ColorPalette::palette = {
    cv::Scalar(0, 255, 0),     cv::Scalar(0, 170, 255), cv::Scalar(0, 128, 255), cv::Scalar(0, 64, 255),  cv::Scalar(0, 0, 255),     cv::Scalar(170, 0, 255),   cv::Scalar(128, 0, 255),
    cv::Scalar(64, 0, 255),    cv::Scalar(0, 0, 255),   cv::Scalar(255, 0, 170), cv::Scalar(255, 0, 128), cv::Scalar(255, 0, 64),    cv::Scalar(255, 128, 0),   cv::Scalar(255, 255, 0),
    cv::Scalar(128, 255, 0),   cv::Scalar(0, 255, 128), cv::Scalar(0, 255, 255), cv::Scalar(0, 128, 128), cv::Scalar(128, 0, 255),   cv::Scalar(255, 0, 255),   cv::Scalar(128, 128, 255),
    cv::Scalar(255, 128, 128), cv::Scalar(255, 64, 64), cv::Scalar(64, 255, 64), cv::Scalar(64, 64, 255), cv::Scalar(128, 255, 255), cv::Scalar(255, 255, 128),
};

cv::Mat preprocessImage(cv::Mat& image, ma::Model* model) {
    int ih = image.rows;
    int iw = image.cols;
    int oh = 0;
    int ow = 0;

    if (model->getInputType() == MA_INPUT_TYPE_IMAGE) {
        oh = reinterpret_cast<const ma_img_t*>(model->getInput())->height;
        ow = reinterpret_cast<const ma_img_t*>(model->getInput())->width;
    }

    cv::Mat resizedImage;
    double resize_scale = std::min((double)oh / ih, (double)ow / iw);
    int nh              = (int)(ih * resize_scale);
    int nw              = (int)(iw * resize_scale);
    cv::resize(image, resizedImage, cv::Size(nw, nh));
    int top    = (oh - nh) / 2;
    int bottom = (oh - nh) - top;
    int left   = (ow - nw) / 2;
    int right  = (ow - nw) - left;

    cv::Mat paddedImage;
    cv::copyMakeBorder(resizedImage, paddedImage, top, bottom, left, right, cv::BORDER_CONSTANT, cv::Scalar::all(0));
    cv::cvtColor(paddedImage, paddedImage, cv::COLOR_BGR2RGB);

    return paddedImage;
}



std::string model_detector(const std::string& model_path) {

    ma_err_t ret = MA_OK;
    auto* engine = new ma::engine::EngineCVI();
    ret          = engine->init();
    if (ret != MA_OK) {
        MA_LOGE(TAG, "engine init failed");
        delete engine;
        return "engine init failed";
    }
    ret = engine->load(model_path);

    MA_LOGI(TAG, "engine load model %s", model_path);
    if (ret != MA_OK) {
        MA_LOGE(TAG, "engine load model failed");
        delete engine;
        return "engine load model failed";
    }

    ma::Model* model = ma::ModelFactory::create(engine);

    if (model == nullptr) {
        MA_LOGE(TAG, "model not supported");
        ma::ModelFactory::remove(model);
        delete engine;
        return "model not supported";
    }

    MA_LOGI(TAG, "model type: %d", model->getType());


    // imread

    ma::Device* device = ma::Device::getInstance();
    ma::Camera* camera = nullptr;

    
    for (auto& sensor : device->getSensors()) {
        if (sensor->getType() == ma::Sensor::Type::kCamera) {
            camera = static_cast<ma::Camera*>(sensor);
            break;
        }
    }
    if (!camera) {
        MA_LOGE(TAG, "No camera sensor found");
        ma::ModelFactory::remove(model);
        delete engine;
        return "No camera sensor found";
    }
    if (camera->init(0) != MA_OK) {  // 假设0是默认模式
        MA_LOGE(TAG, "Camera initialization failed");
        ma::ModelFactory::remove(model);
        delete engine;
        return "Camera initialization failed";
    }

    ma::Camera::CtrlValue value;
    value.i32 = 0;
    if (camera->commandCtrl(ma::Camera::CtrlType::kChannel, ma::Camera::CtrlMode::kWrite, value) != MA_OK) {
        MA_LOGE(TAG, "Failed to set camera channel");
        ma::ModelFactory::remove(model);
        delete engine;
        return "Failed to set camera channel";
    }

    value.u16s[0] = 1920;
    value.u16s[1] = 1080;
    if (camera->commandCtrl(ma::Camera::CtrlType::kWindow, ma::Camera::CtrlMode::kWrite, value) != MA_OK) {
        MA_LOGE(TAG, "Failed to set camera resolution");
        ma::ModelFactory::remove(model);
        delete engine;
        return "Failed to set camera resolution";
    }
    value.i32 = 1;
    if (camera->commandCtrl(ma::Camera::CtrlType::kChannel, ma::Camera::CtrlMode::kWrite, value) != MA_OK) {
        MA_LOGE(TAG, "Failed to set camera channel");
        ma::ModelFactory::remove(model);
        delete engine;
        return "Failed to set camera channel";
    }
    value.u16s[0] = 1920;
    value.u16s[1] = 1080;
    if (camera->commandCtrl(ma::Camera::CtrlType::kWindow, ma::Camera::CtrlMode::kWrite, value) != MA_OK) {
        MA_LOGE(TAG, "Failed to set camera resolution");
        ma::ModelFactory::remove(model);
        delete engine;
        return "Failed to set camera resolution";
    }
    value.i32 = 2;
    if (camera->commandCtrl(ma::Camera::CtrlType::kChannel, ma::Camera::CtrlMode::kWrite, value) != MA_OK) {
        MA_LOGE(TAG, "Failed to set camera channel");
        ma::ModelFactory::remove(model);
        delete engine;
        return "Failed to set camera channel";
    }

    camera->startStream(ma::Camera::StreamMode::kRefreshOnReturn);
    static char buf[8*1024*1024];
    uint32_t count = 0;

    std::string rest="";
    cv::Mat image;
    ma_img_t jpeg;
    if (camera->retrieveFrame(jpeg, MA_PIXEL_FORMAT_JPEG) == MA_OK) {
        MA_LOGI(MA_TAG, "jpeg size: %d", jpeg.size);
        image = cv::imdecode(cv::Mat(1, jpeg.size, CV_8UC1, jpeg.data),cv::IMREAD_COLOR);
        camera->returnFrame(jpeg);
    }

    if (image.empty()) {
        MA_LOGE(TAG, "read image failed");
        
        ma::ModelFactory::remove(model);
        delete engine;
        return "read image failed";
    }

    image = preprocessImage(image, model);

    if (image.empty()) {  // 检查图像是否为空
        printf("preprocessed image is empty");
    }
    ma_img_t img;
    img.data   = (uint8_t*)image.data;
    img.size   = image.rows * image.cols * image.channels();
    img.width  = image.cols;
    img.height = image.rows;
    img.format = MA_PIXEL_FORMAT_RGB888;
    img.rotate = MA_PIXEL_ROTATE_0;


    ma::model::Detector* detector = static_cast<ma::model::Detector*>(model);
    detector->run(&img);
    auto _results = detector->getResults();
    if (_results.empty()) {
        rest+="No objects detected\n";  // 检测结果为空时的输出
    } else {
        for (auto result : _results) {
            rest+="score: " + std::to_string(result.score)+" target: " + std::to_string(result.target);
        }
    }

    auto perf = model->getPerf();
    MA_LOGI(TAG, "pre: %ldms, infer: %ldms, post: %ldms", perf.preprocess, perf.inference, perf.postprocess);

    camera->stopStream();
    if (model) ma::ModelFactory::remove(model);  // 先释放model
    if (engine) delete engine;  // 再释放engine
    return rest;
}