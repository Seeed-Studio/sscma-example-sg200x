#include <chrono>
#include <fstream>
#include <iostream>
#include <numeric>
#include <stdio.h>
#include <string>

#include <opencv2/opencv.hpp>

#include <sscma.h>

#define TAG "main"

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


void drawMaskOnImage(int cls, cv::Mat& targetImage, const std::vector<uint8_t>& maskData, int maskWidth, int maskHeight, double alpha = 0.5) {
    if (maskData.size() != (maskWidth * maskHeight) / 8) {
        throw std::runtime_error("Mask data size is incorrect.");
    }

    cv::Mat maskImage(maskHeight, maskWidth, CV_8UC1, cv::Scalar(0));

    for (int i = 0; i < maskHeight; ++i) {
        for (int j = 0; j < maskWidth; ++j) {
            if (maskData[i * maskWidth / 8 + j / 8] & (1 << (j % 8))) {
                maskImage.at<uchar>(i, j) = 255;
            }
        }
    }

    cv::Mat scaledMaskImage;
    cv::resize(maskImage, scaledMaskImage, cv::Size(targetImage.cols, targetImage.rows), 0, 0, cv::INTER_LINEAR);

    for (int i = 0; i < scaledMaskImage.rows; ++i) {
        for (int j = 0; j < scaledMaskImage.cols; ++j) {
            cv::Vec3b& targetPixel = targetImage.at<cv::Vec3b>(i, j);
            uchar maskPixel        = scaledMaskImage.at<uchar>(i, j);
            if (maskPixel > 0) {
                cv::Vec3b maskColor(ColorPalette::getColor(cls)[0], ColorPalette::getColor(cls)[1], ColorPalette::getColor(cls)[2]);
                targetPixel = (1.0 - alpha) * targetPixel + alpha * maskColor;
            }
        }
    }
}

int main(int argc, char** argv) {

    if (argc < 3) {
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

    MA_LOGI(TAG, "model type: %d", model->getType());

    if (model->getInputType() != MA_INPUT_TYPE_IMAGE) {
        MA_LOGE(TAG, "model input type not supported");
        return 1;
    }

    // imread
    cv::Mat image;
    image = cv::imread(argv[2]);

    if (!image.data) {
        MA_LOGE(TAG, "read image failed");
        return 1;
    }

    image = preprocessImage(image, model);

    ma_img_t img;
    img.data   = (uint8_t*)image.data;
    img.size   = image.rows * image.cols * image.channels();
    img.width  = image.cols;
    img.height = image.rows;
    img.format = MA_PIXEL_FORMAT_RGB888;
    img.rotate = MA_PIXEL_ROTATE_0;

    if (model->getOutputType() == MA_OUTPUT_TYPE_CLASS) {
        ma::model::Classifier* classifier = static_cast<ma::model::Classifier*>(model);
        classifier->setConfig(MA_MODEL_CFG_OPT_THRESHOLD, 0.1f);
        classifier->run(&img);
        auto _results = classifier->getResults();
        for (auto result : _results) {
            char content[100];
            sprintf(content, "%d(%.3f)", result.target, result.score);
            cv::putText(image, content, cv::Point(20, 20), cv::FONT_HERSHEY_SIMPLEX, 0.8, ColorPalette::getColor(result.target), 2, cv::LINE_AA);
            printf("score: %f target: %d\n", result.score, result.target);
        }
    } else if (model->getOutputType() == MA_OUTPUT_TYPE_KEYPOINT) {
        ma::model::PoseDetector* detector = static_cast<ma::model::PoseDetector*>(model);
        detector->run(&img);
        auto _results = detector->getResults();
        for (auto result : _results) {
            printf("x: %f, y: %f, w: %f, h: %f, score: %f target: %d\n", result.box.x, result.box.y, result.box.w, result.box.h, result.box.score, result.box.target);
            for (auto pt : result.pts) {
                printf("x: %f, y: %f\n", pt.x, pt.y);
                cv::circle(image, cv::Point(pt.x * image.cols, pt.y * image.rows), 4, ColorPalette::getColor(result.box.target), -1);
            }

            float x1 = (result.box.x - result.box.w / 2.0) * image.cols;
            float y1 = (result.box.y - result.box.h / 2.0) * image.rows;
            float x2 = (result.box.x + result.box.w / 2.0) * image.cols;
            float y2 = (result.box.y + result.box.h / 2.0) * image.rows;

            char content[100];
            sprintf(content, "%d(%.3f)", result.box.target, result.box.score);

            cv::rectangle(image, cv::Point(x1, y1), cv::Point(x2, y2), ColorPalette::getColor(result.box.target), 2, 8, 0);
            cv::putText(image, content, cv::Point(x1, y1 - 10), cv::FONT_HERSHEY_SIMPLEX, 0.6, ColorPalette::getColor(result.box.target), 2, cv::LINE_AA);
        }
    } else if (model->getOutputType() == MA_OUTPUT_TYPE_SEGMENT) {
        ma::model::Segmentor* segmenter = static_cast<ma::model::Segmentor*>(model);
        segmenter->run(&img);
        auto _results = segmenter->getResults();
        for (auto result : _results) {
            printf("x: %f, y: %f, w: %f, h: %f, score: %f target: %d\n", result.box.x, result.box.y, result.box.w, result.box.h, result.box.score, result.box.target);
            float x1 = (result.box.x - result.box.w / 2.0) * image.cols;
            float y1 = (result.box.y - result.box.h / 2.0) * image.rows;
            float x2 = (result.box.x + result.box.w / 2.0) * image.cols;
            float y2 = (result.box.y + result.box.h / 2.0) * image.rows;

            char content[100];
            sprintf(content, "%d(%.3f)", result.box.target, result.box.score);

            cv::rectangle(image, cv::Point(x1, y1), cv::Point(x2, y2), ColorPalette::getColor(result.box.target), 3, 8, 0);
            cv::putText(image, content, cv::Point(x1, y1 - 10), cv::FONT_HERSHEY_SIMPLEX, 0.6, ColorPalette::getColor(result.box.target), 2, cv::LINE_AA);

            drawMaskOnImage(result.box.target, image, result.mask.data, result.mask.width, result.mask.height);
        }
    } else if (model->getOutputType() == MA_OUTPUT_TYPE_BBOX) {
        ma::model::Detector* detector = static_cast<ma::model::Detector*>(model);
        detector->run(&img);
        auto _results = detector->getResults();
        for (auto result : _results) {
            // cx, cy, w, h
            float x1 = (result.x - result.w / 2.0) * image.cols;
            float y1 = (result.y - result.h / 2.0) * image.rows;
            float x2 = (result.x + result.w / 2.0) * image.cols;
            float y2 = (result.y + result.h / 2.0) * image.rows;

            char content[100];
            sprintf(content, "%d(%.3f)", result.target, result.score);

            cv::rectangle(image, cv::Point(x1, y1), cv::Point(x2, y2), ColorPalette::getColor(result.target), 3, 8, 0);
            cv::putText(image, content, cv::Point(x1, y1 - 10), cv::FONT_HERSHEY_SIMPLEX, 0.6, ColorPalette::getColor(result.target), 2, cv::LINE_AA);

            printf("x: %f, y: %f, w: %f, h: %f, score: %f target: %d\n", result.x, result.y, result.w, result.h, result.score, result.target);
        }
    }

    auto perf = model->getPerf();
    MA_LOGI(TAG, "pre: %ldms, infer: %ldms, post: %ldms", perf.preprocess, perf.inference, perf.postprocess);

    cv::cvtColor(image, image, cv::COLOR_RGB2BGR);

    if (argc >= 4) {
        cv::imwrite(argv[3], image);
    } else {
        cv::imwrite("result.jpg", image);
    }

    ma::Thread::sleep(ma::Tick::fromMilliseconds(100));

    ma::ModelFactory::remove(model);

    return 0;
}