#include <chrono>
#include <fstream>
#include <iostream>
#include <numeric>
#include <stdio.h>
#include <string>

#include <opencv2/opencv.hpp>

#include <sscma.h>

#define TAG "main"

int main(int argc, char** argv) {


    if (argc != 4) {
        printf("Usage:\n");
        printf("   %s cvimodel image.jpg image_detected.jpg\n", argv[0]);
        printf("ex: %s yolo11.cvimodel cat.jpg out.jpg \n", argv[0]);
        exit(-1);
    }
    ma_err_t ret = MA_OK;
    auto* engine = new ma::engine::EngineCVI();
    ret          = engine->init();
    if (ret != MA_OK) {
        MA_LOGE(TAG, "engine init failed");
        return 1;
    }
    ret = engine->load(argv[1]);

    MA_LOGI(TAG, "engine load model %s", argv[1]);
    if (ret != MA_OK) {
        MA_LOGE(TAG, "engine load model failed");
        return 1;
    }

    ma::Model* model = ma::ModelFactory::create(engine);
    if (model == nullptr) {
        MA_LOGE(TAG, "model not supported");
        return 1;
    }

    // imread
    cv::Mat image;
    image = cv::imread(argv[2]);

    if (!image.data) {
        MA_LOGE(TAG, "read image failed");
        return 1;
    }

    cv::Mat cloned = image.clone();


    if (model->getType() == ma_model_type_t::MA_MODEL_TYPE_IMCLS) {

        ma::model::Classifier* classifier = static_cast<ma::model::Classifier*>(model);

        int width  = classifier->getInputImg()->width;
        int height = classifier->getInputImg()->height;

        // resize & letterbox
        int ih              = image.rows;
        int iw              = image.cols;
        int oh              = height;
        int ow              = width;
        double resize_scale = std::min((double)oh / ih, (double)ow / iw);
        int nh              = (int)(ih * resize_scale);
        int nw              = (int)(iw * resize_scale);
        cv::resize(image, image, cv::Size(nw, nh));
        int top    = (oh - nh) / 2;
        int bottom = (oh - nh) - top;
        int left   = (ow - nw) / 2;
        int right  = (ow - nw) - left;
        cv::copyMakeBorder(image, image, top, bottom, left, right, cv::BORDER_CONSTANT, cv::Scalar::all(0));
        cv::cvtColor(image, image, cv::COLOR_BGR2RGB);

        ma_img_t img;
        img.data   = (uint8_t*)image.data;
        img.size   = image.rows * image.cols * image.channels();
        img.width  = image.cols;
        img.height = image.rows;
        img.format = classifier->getInputImg()->format;
        img.rotate = MA_PIXEL_ROTATE_0;

        classifier->run(&img);

        auto perf = classifier->getPerf();
        MA_LOGI(TAG, "pre: %ldms, infer: %ldms, post: %ldms", perf.preprocess, perf.inference, perf.postprocess);
        auto _results = classifier->getResults();
        for (auto result : _results) {
            char content[100];
            sprintf(content, "%d(%.3f)", result.target, result.score);
            cv::putText(cloned, content, cv::Point(10, 10), cv::FONT_HERSHEY_DUPLEX, 0.8, cv::Scalar(0, 0, 255), 2);
            printf("score: %f target: %d\n", result.score, result.target);
        }
    } else {
        ma::model::Detector* detector = static_cast<ma::model::Detector*>(model);

        int width  = detector->getInputImg()->width;
        int height = detector->getInputImg()->height;

        // resize & letterbox
        int ih              = image.rows;
        int iw              = image.cols;
        int oh              = height;
        int ow              = width;
        double resize_scale = std::min((double)oh / ih, (double)ow / iw);
        int nh              = (int)(ih * resize_scale);
        int nw              = (int)(iw * resize_scale);
        cv::resize(image, image, cv::Size(nw, nh));
        int top    = (oh - nh) / 2;
        int bottom = (oh - nh) - top;
        int left   = (ow - nw) / 2;
        int right  = (ow - nw) - left;
        cv::copyMakeBorder(image, image, top, bottom, left, right, cv::BORDER_CONSTANT, cv::Scalar::all(0));
        cv::cvtColor(image, image, cv::COLOR_BGR2RGB);

        ma_img_t img;
        img.data   = (uint8_t*)image.data;
        img.size   = image.rows * image.cols * image.channels();
        img.width  = image.cols;
        img.height = image.rows;
        img.format = detector->getInputImg()->format;
        img.rotate = MA_PIXEL_ROTATE_0;

        detector->run(&img);
        auto perf = detector->getPerf();
        MA_LOGI(TAG, "pre: %ldms, infer: %ldms, post: %ldms", perf.preprocess, perf.inference, perf.postprocess);
        auto _results = detector->getResults();
        int restored_w;
        int restored_h;
        float resize_ratio;
        for (auto result : _results) {

            // if (float(cloned.cols) / img.width > float(cloned.rows) / img.height) {
            //     restored_w = cloned.cols;
            //     restored_h = (int)((float)cloned.cols / img.width * img.height);
            //     resize_ratio = (float)cloned.cols / img.width;
            // }else{
            //     restored_w = (int)((float)cloned.rows / img.height * img.width);
            //     restored_h = cloned.rows;
            //     resize_ratio = (float)cloned.rows / img.height;
            // }
            // cx, cy, w, h
            float x1 = (result.x - result.w / 2.0) * image.cols;
            float y1 = (result.y - result.h / 2.0) * image.rows;
            float x2 = (result.x + result.w / 2.0) * image.cols;
            float y2 = (result.y + result.h / 2.0) * image.rows;

            // ma_printf("x1: %f, y1: %f, x2: %f, y2: %f, cols: %d, rows: %d\n", x1, y1, x2, y2, cloned.cols, cloned.rows);


            char content[100];
            sprintf(content, "%d(%.3f)", result.target, result.score);

            cv::rectangle(image, cv::Point(x1, y1), cv::Point(x2, y2), cv::Scalar(255, 255, 0), 3, 8, 0);
            cv::putText(image, content, cv::Point(x1, y1), cv::FONT_HERSHEY_DUPLEX, 0.6, cv::Scalar(0, 0, 255), 2);

            printf("x: %f, y: %f, w: %f, h: %f, score: %f target: %d\n", result.x, result.y, result.w, result.h, result.score, result.target);
        }
    }

    cv::cvtColor(image, image, cv::COLOR_RGB2BGR);
    cv::imwrite(argv[3], image);

    ma::ModelFactory::remove(model);


    return 0;
}