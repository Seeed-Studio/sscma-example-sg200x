#ifndef API_DEVICE_H
#define API_DEVICE_H

#include "api_base.h"

class api_device : public api_base {
private:
    static api_status_t queryDeviceInfo(const json& request, json& response);
    static api_status_t queryServiceStatus(const json& request, json& response);

public:
    api_device()
        : api_base("deviceMgr")
    {
        MA_LOGV("");
        REG_API_NO_AUTH(queryDeviceInfo);
        REG_API(queryServiceStatus);
    }

    ~api_device()
    {
        MA_LOGV("");
    }
};

#endif // API_DEVICE_H
