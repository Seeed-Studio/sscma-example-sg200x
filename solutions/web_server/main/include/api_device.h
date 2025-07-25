#ifndef API_DEVICE_H
#define API_DEVICE_H

#include "api_base.h"
#include <stdexcept>

class api_device : public api_base {
private:
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

public:
    api_device()
        : api_base("deviceMgr")
    {
        LOGV("");
        REG_API(getCameraWebsocketUrl);

        REG_API(getDeviceInfo);
        REG_API_NO_AUTH(getDeviceList);
        REG_API(updateDeviceName);
        REG_API(queryDeviceInfo); // fixed: no auth
        REG_API(getSystemStatus);
        REG_API(queryServiceStatus);
        REG_API(setPower);

        REG_API(getAppInfo);
        REG_API(uploadApp);

        REG_API(getModelFile);
        REG_API_NO_AUTH(getModelInfo); // fixed: no auth
        REG_API(getModelList); // fixed: no auth
        REG_API(uploadModel); // fixed: no auth

        REG_API_NO_AUTH(getPlatformInfo);
        REG_API(savePlatformInfo);

        REG_API(updateChannel);
        REG_API(cancelUpdate);
        REG_API(getSystemUpdateVersion);
        REG_API(getUpdateProgress);
        REG_API(updateSystem);
    }

    ~api_device()
    {
        LOGV("");
    }
};

#endif // API_DEVICE_H
