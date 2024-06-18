#include <chrono>
#include <fstream>
#include <iostream>
#include <numeric>
#include <stdio.h>
#include <string>

#include <sscma.h>

#include "img_meter.h"

#define TAG "main"

int main(int argc, char** argv) {

    ma_err_t ret = MA_OK;
    auto* engine = new EngineCVI();
    ret          = engine->init();
    if (ret != MA_OK) {
        MA_LOGE(TAG, "engine init failed");
        return 1;
    }
    ret = engine->loadModel(argv[1]);

    MA_LOGI(TAG, "engine load model %s", argv[1]);
    if (ret != MA_OK) {
        MA_LOGE(TAG, "engine load model failed");
        return 1;
    }
    ma_tensor_t input  = engine->getInput(0);
    ma_tensor_t output = engine->getOutput(0);

    printf("input tensor : %s\n", input.name);
    printf("shape: ");
    for (int i = 0; i < input.shape.size; i++) {
        printf("%d ", input.shape.dims[i]);
    }
    printf("\n");
    printf("size: %d\n", input.size);
    printf("type: %d\n", input.type);
    printf("scale: %f\n", input.quant_param.scale);
    printf("zero_point: %d\n", input.quant_param.zero_point);

    printf("output tensor: %s\n", output.name);
    printf("shape: ");
    for (int i = 0; i < output.shape.size; i++) {
        printf("%d ", output.shape.dims[i]);
    }
    printf("\n");
    printf("size: %d\n", output.size);
    printf("type: %d\n", output.type);
    printf("scale: %f\n", output.quant_param.scale);
    printf("zero_point: %d\n", output.quant_param.zero_point);

    ma::model::Detector* detector = static_cast<ma::model::Detector*>(new ma::model::Yolo(engine));

    ma_img_t img;
    img.data   = (uint8_t*)gImage_meter;
    img.size   = sizeof(gImage_meter);
    img.width  = 240;
    img.height = 240;
    img.format = MA_PIXEL_FORMAT_RGB888;
    img.rotate = MA_PIXEL_ROTATE_0;

    detector->setConfig(MA_MODEL_CFG_OPT_THRESHOLD, 0.6f);
    detector->setConfig(MA_MODEL_CFG_OPT_NMS, 0.2f);

    while (1) {

        detector->run(&img);

        auto perf = detector->getPerf();
        MA_LOGI(TAG,
                "pre: %ldms, infer: %ldms, post: %ldms",
                perf.preprocess,
                perf.inference,
                perf.postprocess);
        auto _results = detector->getResults();
        int value     = 0;
        for (int i = 0; i < _results.size(); i++) {
            auto result = _results[i];
            MA_LOGI(TAG,
                    "class: %d, score: %f, x: %f, y: %f, w: %f, h: %f",
                    result.target,
                    result.score,
                    result.x,
                    result.y,
                    result.w,
                    result.h);
            value = value * 10 + result.target;
        }
    }

    return 0;
}