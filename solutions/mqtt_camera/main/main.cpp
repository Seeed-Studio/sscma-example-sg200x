#include <sscma.h>

#define TAG "main"

int main(int argc, char** argv) {

    MA_LOGD(TAG, "build %s %s", __DATE__, __TIME__);

    ma_err_t ret = MA_OK;
    auto* engine = new EngineCVI();
    ret          = engine->init();
    if (ret != MA_OK) {
        MA_LOGE(TAG, "engine init failed");
        return 1;
    }
    ret = engine->loadModel(argv[1]);

    ma::model::Detector* detector = static_cast<ma::model::Detector*>(new ma::model::Yolo(engine));
    detector->setConfig(MA_MODEL_CFG_OPT_THRESHOLD, 0.6f);
    detector->setConfig(MA_MODEL_CFG_OPT_NMS, 0.2f);


    MA_LOGI(TAG, "engine load model %s", argv[1]);
    if (ret != MA_OK) {
        MA_LOGE(TAG, "engine load model failed");
        return 1;
    }

    CameraMQTT camera("camera", "sscma/v0/camera/tx", "sscma/v0/camera/rx");

    if (camera.connect("127.0.0.1", 1883) == MA_OK) {
        MA_LOGI(TAG, "camera connected");
    } else {
        MA_LOGE(TAG, "camera connect failed");
        return -1;
    }

    while (!camera) {
        Tick::sleep(Tick::fromMilliseconds(10));
        MA_LOGI(TAG, ".");
    }


    if (camera.open() == MA_OK) {
        MA_LOGI(TAG, "camera open");
    } else {
        MA_LOGE(TAG, "camera open failed");
        return -1;
    }

    if (camera.start() == MA_OK) {
        MA_LOGI(TAG, "camera start");
    } else {
        MA_LOGE(TAG, "camera start failed");
        return -1;
    }

    int count = 0;
    double fps      = 0;
    ma_tick_t start = Tick::current();

    while (1) {
        ma_img_t img;
        if (camera.read(&img) == MA_OK) {
            count++;
            if(Tick::current() - start > Tick::fromSeconds(1)) {
                 start              = Tick::current();
                 fps                = count;
                 count              = 0;
            }
            MA_LOGD(TAG, "fps: %f", fps);
            // detector->run(&img);
            // auto perf = detector->getPerf();
            // MA_LOGI(TAG,
            //         "fps: %f %d, pre: %ldms, infer: %ldms, post: %ldms",
            //         fps,
            //         interval,
            //         perf.preprocess,
            //         perf.inference,
            //         perf.postprocess);
            // auto _results = detector->getResults();
            // int value     = 0;
            // for (int i = 0; i < _results.size(); i++) {
            //     auto result = _results[i];
            //     MA_LOGI(TAG,
            //             "class: %d, score: %f, x: %f, y: %f, w: %f, h: %f",
            //             result.target,
            //             result.score,
            //             result.x,
            //             result.y,
            //             result.w,
            //             result.h);
            //     value = value * 10 + result.target;
            // }
            camera.release(&img);
        }
    }
    camera.stop();


    return 0;
}