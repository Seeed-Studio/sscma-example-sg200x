#ifndef API_DEVICE_H
#define API_DEVICE_H

#include <memory>
#include <stdexcept>
#include <mutex>

#include "api_base.h"
#include "serviced.h"

class api_device : public api_base {
private:
    static inline std::unique_ptr<serviced> _serviced;

    static api_status_t getCameraWebsocketUrl(request_t req, response_t res);

    static api_status_t getDeviceInfo(request_t req, response_t res);
    static api_status_t getDeviceList(request_t req, response_t res);
    static api_status_t updateDeviceName(request_t req, response_t res);
    static api_status_t queryDeviceInfo(request_t req, response_t res);
    static api_status_t getSystemStatus(request_t req, response_t res);
    static api_status_t queryServiceStatus(request_t req, response_t res);
    static api_status_t setPower(request_t req, response_t res);

    static api_status_t getAppInfo(request_t req, response_t res);
    static api_status_t uploadApp(request_t req, response_t res);

    static api_status_t getModelFile(request_t req, response_t res);
    static api_status_t getModelInfo(request_t req, response_t res);
    static api_status_t getModelList(request_t req, response_t res);
    static api_status_t uploadModel(request_t req, response_t res);

    static api_status_t getPlatformInfo(request_t req, response_t res);
    static api_status_t savePlatformInfo(request_t req, response_t res);

    static api_status_t updateChannel(request_t req, response_t res);
    static api_status_t cancelUpdate(request_t req, response_t res);
    static api_status_t getSystemUpdateVersion(request_t req, response_t res);
    static api_status_t getUpdateProgress(request_t req, response_t res);
    static api_status_t updateSystem(request_t req, response_t res);

    // Timestamp APIs
    static api_status_t setTimestamp(request_t req, response_t res);
    static api_status_t getTimestamp(request_t req, response_t res);
    static api_status_t setTimezone(request_t req, response_t res);
    static api_status_t getTimezone(request_t req, response_t res);
    static api_status_t getTimezoneList(request_t req, response_t res);

    static api_status_t queryBatteryInfo(request_t req, response_t res);

public:
    api_device()
        : api_base("deviceMgr")
    {
        _serviced = std::make_unique<serviced>();
        if (_serviced == nullptr) {
            throw std::runtime_error("Failed to create serviced");
            return;
        }

        _dev_info = parse_result(script(__func__));
        LOGD("%s", _dev_info.dump().c_str());

        try {
            _model_dir = _dev_info["model"]["preset"];
            _model_suffix = _dev_info["model"]["file"];
            _model_suffix = _model_suffix.substr(_model_suffix.find_last_of("."));
        } catch (const json::exception& e) {
            LOGE("Invalid model preset.");
            _model_dir = "";
            _model_suffix = "";
        }

        REG_API(getCameraWebsocketUrl);

        REG_API(getDeviceInfo);
        REG_API_NO_AUTH(getDeviceList);
        REG_API(updateDeviceName);
        REG_API_NO_AUTH(queryDeviceInfo);
        REG_API(getSystemStatus);
        REG_API_NO_AUTH(queryServiceStatus);
        REG_API(setPower);

        REG_API(getAppInfo);
        REG_API(uploadApp);

        REG_API_NO_AUTH(getModelFile);
        REG_API_NO_AUTH(getModelInfo);
        REG_API_NO_AUTH(getModelList);
        REG_API_NO_AUTH(uploadModel);

        REG_API_NO_AUTH(getPlatformInfo);
        REG_API(savePlatformInfo);

        REG_API(updateChannel);
        REG_API(cancelUpdate);
        REG_API(getSystemUpdateVersion);
        REG_API(getUpdateProgress);
        REG_API(updateSystem);

        REG_API(setTimestamp);
        REG_API(getTimestamp);
        REG_API(setTimezone);
        REG_API(getTimezone);
        REG_API(getTimezoneList);

        REG_API_NO_AUTH(queryBatteryInfo);
    }

    ~api_device()
    {
        LOGV("");
    }

private:
    static inline json _dev_info;
    static inline std::string _model_dir = "";
    static inline std::string _model_suffix = "";
    static inline std::mutex _battery_mutex;
};

#endif // API_DEVICE_H
