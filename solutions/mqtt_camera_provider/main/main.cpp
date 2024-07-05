#include <sscma.h>

#include "img_meter.h"

#define TAG "main"


ma_err_t callback(ma_camera_event_t event) {
    MA_LOGI(TAG, "event: %d", event);
    return MA_OK;
}

int main(int argc, char** argv) {

    MA_LOGD(TAG, "build %s %s", __DATE__, __TIME__);

    CameraMQTTProvider camera("camer2a", "sscma/v0/camera/rx", "sscma/v0/camera/tx");

    if (camera.connect("127.0.0.1", 1883) == MA_OK) {
        MA_LOGI(TAG, "camera connected");
    } else {
        MA_LOGE(TAG, "camera connect failed");
    }

    camera.setCallback(callback);

    uint32_t count     = 0;
    double fps         = 0;
    ma_tick_t start    = Tick::current();
    uint8_t* fake_data = static_cast<uint8_t*>(ma_malloc(640 * 640 * 3));
    while (1) {
        if (camera.isStreaming()) {
            count++;
            if (Tick::current() - start >= Tick::fromSeconds(1)) {
                fps   = count;
                count = 0;
                start = Tick::current();
            }
            MA_LOGI(TAG, "fps: %f count: %d", fps, count);
            ma_img_t img;
            img.data   = fake_data;
            img.size   = 640 * 640 * 3;
            img.width  = 640;
            img.height = 640;
            img.format = MA_PIXEL_FORMAT_RGB888;
            img.rotate = MA_PIXEL_ROTATE_0;
            camera.write(&img);
            Tick::sleep(Tick::fromMilliseconds(30));  // 100fps
        }
    }


    return 0;
}