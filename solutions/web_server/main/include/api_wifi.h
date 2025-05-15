#ifndef API_WIFI_H
#define API_WIFI_H

#include "api_base.h"

class api_wifi : public api_base {
public:
    api_wifi()
        : api_base("wifiMgr")
    {
        printf("%s,%d\n", __func__, __LINE__);
    }

    ~api_wifi()
    {
        printf("%s,%d\n", __func__, __LINE__);
    }
};

#endif // API_WIFI_H
