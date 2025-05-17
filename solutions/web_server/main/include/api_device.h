#ifndef API_DEVICE_H
#define API_DEVICE_H

#include "api_base.h"

class api_device : public api_base {
private:
    static api_status_t cancelUpdate(const json& request, json& response);
    static api_status_t getAppInfo(const json& request, json& response);
    static api_status_t getDeviceInfo(const json& request, json& response);
    static api_status_t getDeviceList(const json& request, json& response);
    static api_status_t getModelFile(const json& request, json& response);
    static api_status_t getModelInfo(const json& request, json& response);
    static api_status_t getModelList(const json& request, json& response);
    static api_status_t getPlatformInfo(const json& request, json& response);
    static api_status_t getSystemStatus(const json& request, json& response);
    static api_status_t getSystemUpdateVesionInfo(const json& request, json& response);
    static api_status_t getUpdateProgress(const json& request, json& response);
    static api_status_t queryDeviceInfo(const json& request, json& response);
    static api_status_t queryServiceStatus(const json& request, json& response);
    static api_status_t savePlatformInfo(const json& request, json& response);
    static api_status_t setPower(const json& request, json& response);
    static api_status_t updateChannel(const json& request, json& response);
    static api_status_t updateDeviceName(const json& request, json& response);
    static api_status_t updateSystem(const json& request, json& response);
    static api_status_t uploadApp(const json& request, json& response);
    static api_status_t uploadModel(const json& request, json& response);

public:
    api_device()
        : api_base("deviceMgr")
    {
        MA_LOGV("");

        REG_API(cancelUpdate);
        REG_API(getAppInfo);
        REG_API(getDeviceInfo);
        REG_API_NO_AUTH(getDeviceList);
        REG_API(getModelFile);
        REG_API(getModelInfo); // fixed: no auth
        REG_API(getModelList); // fixed: no auth
        REG_API(getPlatformInfo);
        REG_API(getSystemStatus);
        REG_API(getSystemUpdateVesionInfo);
        REG_API(getUpdateProgress);
        REG_API(queryDeviceInfo); // fixed: no auth
        REG_API(queryServiceStatus);
        REG_API(savePlatformInfo);
        REG_API(setPower);
        REG_API(updateChannel);
        REG_API(updateDeviceName);
        REG_API(updateSystem);
        REG_API(uploadApp);
        REG_API(uploadModel); // fixed: no auth
    }

    ~api_device()
    {
        MA_LOGV("");
    }
};

#endif // API_DEVICE_H
