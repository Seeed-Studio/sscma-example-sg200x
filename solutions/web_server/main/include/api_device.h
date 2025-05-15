#ifndef API_DEVICE_H
#define API_DEVICE_H

#include "api_base.h"

class api_device : public api_base {
private:
    static api_status_t queryDeviceInfo(const json& request, json& response);

public:
    api_device()
        : api_base("deviceMgr")
    {
        printf("%s,%d\n", __func__, __LINE__);
        REG_API_FULL("", queryDeviceInfo, true);
        REG_API_NO_AUTH(queryDeviceInfo);
    }

    ~api_device()
    {
        printf("%s,%d\n", __func__, __LINE__);
    }
};

#endif // API_DEVICE_H
