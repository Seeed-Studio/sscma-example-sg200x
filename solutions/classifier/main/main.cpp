#include <chrono>
#include <fstream>
#include <iostream>
#include <numeric>
#include <stdio.h>
#include <string>

#include <sscma.h>

#define TAG "main"

bool exit_flag = false;

void handle_sigint(int sig) {
    printf("Caught signal %d\n", sig);
    // Clean up code here
    exit_flag = true;
}

int main(int argc, char** argv) {

    signal(SIGINT, handle_sigint);
    ma_err_t ret = MA_OK;
    auto* engine = new ma::engine::EngineCVI();
    ret          = engine->init();
    if (ret != MA_OK) {
        MA_LOGE(TAG, "engine init failed");
        return 1;
    }
    ret = engine->load(argv[1]);
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

    while (!exit_flag) {
        uint32_t start = std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::system_clock::now().time_since_epoch())
                             .count();
        ret          = engine->run();
        uint32_t end = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count();
        if (ret != MA_OK) {
            MA_LOGE(TAG, "engine run failed");
            return 1;
        }

        printf("run time: %d ms\n", end - start);

    }

    //engine->deinit();
    return 0;
}