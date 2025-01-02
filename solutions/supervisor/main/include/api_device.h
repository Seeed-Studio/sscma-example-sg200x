#ifndef _API_DEVICE_H_
#define _API_DEVICE_H_

#include "api_base.h"

class api_device : public api_base {
public:
    api_device(const supervisor* sv)
        : api_base(sv, "/usr/share/supervisor/scripts/devicetool.sh")
    {
        system_status_ = exec_shell_cmd("getSystemStatus").empty()
            ? APP_STATUS_NORMAL
            : APP_STATUS_FAILED;

        register_apis();
    }

private:
    app_status_t system_status_;

    void register_apis() override;

    int queryServiceStatus(HttpRequest* req, HttpResponse* resp);
    int queryDeviceInfo(HttpRequest* req, HttpResponse* resp);
    int updateDeviceName(HttpRequest* req, HttpResponse* resp);
    int updateChannel(HttpRequest* req, HttpResponse* resp);
    int setPower(HttpRequest* req, HttpResponse* resp);
};

#endif // _API_DEVICE_H_
