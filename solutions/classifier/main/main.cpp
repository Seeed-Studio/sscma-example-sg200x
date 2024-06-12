#include <chrono>
#include <fstream>
#include <iostream>
#include <numeric>
#include <stdio.h>
#include <string>

#include <sscma.h>

#define TAG "main"

int main(int argc, char** argv) {

    ma_err_t ret    = MA_OK;
    auto*    engine = new EngineCVI();
    ret             = engine->init();
    if (ret != MA_OK) {
        MA_LOGE(TAG, "engine init failed");
        return 1;
    }
    ret = engine->loadModel("mobilenet_v2.cvimodel");
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

    ret = engine->run();
    if (ret != MA_OK) {
        MA_LOGE(TAG, "engine run failed");
        return 1;
    }

    return 0;
}