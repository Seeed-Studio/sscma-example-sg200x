#include <iostream>

#include <sscma.h>
#include <video.h>
#include "ma_transport_rtsp.h"

int main(int argc, char** argv) {

    Device* device = Device::getInstance();
    Camera* camera = nullptr;

    Signal::install({SIGINT, SIGSEGV, SIGABRT, SIGTRAP, SIGTERM, SIGHUP, SIGQUIT, SIGPIPE}, [device](int sig) {
        std::cout << "Caught signal " << sig << std::endl;
        for (auto& sensor : device->getSensors()) {
            sensor->deInit();
        }
        exit(0);
    });

    TransportRTSP rtsp;
    TransportRTSP::RTSPConfig rtsp_config;
    rtsp_config.port = 554;
    rtsp_config.format = MA_PIXEL_FORMAT_H264;
    rtsp_config.name = "test";
    rtsp_config.user = "admin";
    rtsp_config.pass = "admin";
    rtsp.init(&rtsp_config);


    for (auto& sensor : device->getSensors()) {
        if (sensor->getType() == ma::Sensor::Type::kCamera) {
            camera = static_cast<Camera*>(sensor);
            camera->init(0);
            Camera::CtrlValue value;
            value.i32 = 0;
            camera->commandCtrl(Camera::CtrlType::kChannel, Camera::CtrlMode::kWrite, value);
            value.u16s[0] = 640;
            value.u16s[1] = 640;
            camera->commandCtrl(Camera::CtrlType::kWindow, Camera::CtrlMode::kWrite, value);
            value.i32 = 1;
            camera->commandCtrl(Camera::CtrlType::kChannel, Camera::CtrlMode::kWrite, value);
            value.u16s[0] = 640;
            value.u16s[1] = 640;
            camera->commandCtrl(Camera::CtrlType::kWindow, Camera::CtrlMode::kWrite, value);
            value.i32 = 2;
            camera->commandCtrl(Camera::CtrlType::kChannel, Camera::CtrlMode::kWrite, value);
            break;
        }
    }


    camera->startStream(Camera::StreamMode::kRefreshOnReturn);
    static char buf[4 * 1024];
    while (true) {
        for(auto & transport : device->getTransports()) {
            if(*transport && transport->available() > 0) {
                memset(buf, 0, sizeof(buf));
                int len = transport->receive(buf, sizeof(buf));
                transport->send(buf, len);
            }
        }
        ma_img_t frame;
        if (camera->retrieveFrame(frame, MA_PIXEL_FORMAT_H264) == MA_OK) {
            MA_LOGI(MA_TAG, "frame size: %d", frame.size);
            rtsp.send(reinterpret_cast<char*>(frame.data), frame.size);
            camera->returnFrame(frame);
        }
        // ma_img_t jpeg;
        // if (camera->retrieveFrame(jpeg, MA_PIXEL_FORMAT_JPEG) == MA_OK) {
        //     MA_LOGI(MA_TAG, "jpeg size: %d", jpeg.size);
        //     camera->returnFrame(jpeg);
        // }
        // ma_img_t raw;
        // if (camera->retrieveFrame(raw, MA_PIXEL_FORMAT_RGB888) == MA_OK) {
        //     MA_LOGI(MA_TAG, "raw size: %d", raw.size);
        //     camera->returnFrame(raw);
    }
        // }

    camera->stopStream();

    return 0;
}