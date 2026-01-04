#include <iostream>
#include <chrono>
#include <thread>
#include <iterator>

#include <sscma.h>
#include <video.h>

using namespace ma;

#define TAG "model_detector"

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage:\n");
        printf("   %s cvimodel [threshold]\n", argv[0]);
        printf("ex: %s yolo11.cvimodel 0.5\n", argv[0]);
        exit(-1);
    }

    float threshold = 0.5f; // default threshold
    if (argc >= 3) {
        threshold = atof(argv[2]);
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
    MA_LOGI(TAG, "threshold: %f", threshold);

    if (model->getInputType() != MA_INPUT_TYPE_IMAGE) {
        MA_LOGE(TAG, "model input type not supported");
        return 1;
    }

    // Get model input dimensions
    const ma_img_t* model_input = static_cast<const ma_img_t*>(model->getInput());
    int input_width = model_input->width;
    int input_height = model_input->height;
    MA_LOGI(TAG, "model input size: %dx%d", input_width, input_height);

    // Initialize device and camera
    Device* device = Device::getInstance();
    Camera* camera = nullptr;

    Signal::install({SIGINT, SIGSEGV, SIGABRT, SIGTRAP, SIGTERM, SIGHUP, SIGQUIT, SIGPIPE}, [device](int sig) {
        std::cout << "Caught signal " << sig << std::endl;
        for (auto& sensor : device->getSensors()) {
            sensor->deInit();
        }
        exit(0);
    });

    for (auto& sensor : device->getSensors()) {
        if (sensor->getType() == ma::Sensor::Type::kCamera) {
            camera = static_cast<Camera*>(sensor);
            camera->init(0);
            Camera::CtrlValue value;
            value.i32 = 0;
            camera->commandCtrl(Camera::CtrlType::kChannel, Camera::CtrlMode::kWrite, value);
            value.u16s[0] = input_width;
            value.u16s[1] = input_height;
            camera->commandCtrl(Camera::CtrlType::kWindow, Camera::CtrlMode::kWrite, value);
            value.i32 = 1;
            camera->commandCtrl(Camera::CtrlType::kPhysical, Camera::CtrlMode::kWrite, value);
            break;
        }
    }

    if (!camera) {
        MA_LOGE(TAG, "No camera found");
        return 1;
    }

    camera->startStream(Camera::StreamMode::kRefreshOnReturn);

    int frame_count = 0;
    long long total_time = 0;

    while (true) {
        ma_img_t frame;
        if (camera->retrieveFrame(frame, MA_PIXEL_FORMAT_RGB888) == MA_OK) {
            auto capture_start = std::chrono::high_resolution_clock::now();

            ma_tensor_t tensor = {
                .size        = frame.size,
                .is_physical = true,  // assume physical address
                .is_variable = false,
            };
            tensor.data.data = reinterpret_cast<void*>(frame.data);

            engine->setInput(0, tensor);
            auto capture_end = std::chrono::high_resolution_clock::now();

            model->setPreprocessDone([camera, &frame](void* ctx) { camera->returnFrame(frame); });

            if (model->getOutputType() == MA_OUTPUT_TYPE_BBOX) {
                ma::model::Detector* detector = static_cast<ma::model::Detector*>(model);
                detector->setConfig(MA_MODEL_CFG_OPT_THRESHOLD, threshold);

                auto inference_start = std::chrono::high_resolution_clock::now();
                detector->run(nullptr);
                auto _results = detector->getResults();
                auto inference_end = std::chrono::high_resolution_clock::now();

                auto capture_duration = std::chrono::duration_cast<std::chrono::milliseconds>(capture_end - capture_start);
                auto inference_duration = std::chrono::duration_cast<std::chrono::milliseconds>(inference_end - inference_start);
                auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(inference_end - capture_start);

                total_time += total_duration.count();
                frame_count++;

                printf("Frame %d: Capture: %lld ms, Inference: %lld ms, Total: %lld ms\n",
                       frame_count, capture_duration.count(), inference_duration.count(), total_duration.count());

                size_t num_detections = std::distance(_results.begin(), _results.end());
                printf("Detections: %zu objects found\n", num_detections);
                for (auto result : _results) {
                    printf("- Class %d: %.2f confidence at [%.0f,%.0f,%.0f,%.0f]\n", result.target, result.score, result.x * input_width, result.y * input_height, (result.x + result.w) * input_width, (result.y + result.h) * input_height);
                }

                if (frame_count % 10 == 0) {
                    double avg_time = static_cast<double>(total_time) / frame_count;
                    printf("Average processing time per frame: %.2f ms (over %d frames)\n", avg_time, frame_count);
                }
            } else {
                MA_LOGW(TAG, "Model output type not supported for detection, only bbox supported in this example");
                camera->returnFrame(frame);
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    camera->stopStream();
    ma::ModelFactory::remove(model);

    return 0;
}