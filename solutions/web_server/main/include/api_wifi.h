#ifndef API_WIFI_H
#define API_WIFI_H

#include "api_base.h"

class api_wifi : public api_base {
public:
    api_wifi()
        : api_base("wifiMgr")
    {
        MA_LOGV("");
    }

    ~api_wifi()
    {
        MA_LOGV("");
    }
};

#endif // API_WIFI_H
