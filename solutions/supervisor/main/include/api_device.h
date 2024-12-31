#ifndef _API_DEVICE_H_
#define _API_DEVICE_H_

#include "api_base.h"

class api_device : public api_base {
public:
    api_device()
        : api_base("/usr/share/supervisor/scripts/devicetool.sh")
    {
        register_apis();
    }

private:
    void register_apis() override;

    int getSystemStatus(HttpRequest* req, HttpResponse* resp);
    int queryDeviceInfo(HttpRequest* req, HttpResponse* resp);
};

#endif // _API_DEVICE_H_
