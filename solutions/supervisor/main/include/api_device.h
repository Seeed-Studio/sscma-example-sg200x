#ifndef _API_DEVICE_H_
#define _API_DEVICE_H_

#include "api_base.h"

#undef TAG
#define TAG "api_device"

class api_device : public api_base {
public:
    api_device(const std::string& path = "/usr/share/supervisor/scripts/devicetool.sh")
        : api_base(path)
    {
        system_status_ = run_script("getSystemStatus").empty()
            ? APP_STATUS_NORMAL
            : APP_STATUS_FAILED;
        MA_LOGI(TAG, "system_status: %d", system_status_);

        sn_ = run_script("getSnCode");
        MA_LOGI(TAG, "sn: %s", sn_.c_str());

        std::string os_info = run_script("getOsInfo");
        if (!os_info.empty()) {
            size_t pos = os_info.find(' ');
            if (pos != std::string::npos) {
                os_name_ = os_info.substr(0, pos);
                os_version_ = os_info.substr(pos + 1);
            }
        }
        MA_LOGI(TAG, "os_name: %s os_version: %s", os_name_.c_str(), os_version_.c_str());

        run_script("getModelInfo", "1&");

        register_apis();
    }

    std::function<app_status_t()> sscma_status;
    std::function<app_status_t()> nodered_status;

private:
    app_status_t system_status_;
    std::string sn_;
    std::string os_name_;
    std::string os_version_;

    void register_apis() override;

    int queryServiceStatus(HttpRequest* req, HttpResponse* resp);
    int queryDeviceInfo(HttpRequest* req, HttpResponse* resp);
    int updateDeviceName(HttpRequest* req, HttpResponse* resp);
    int updateChannel(HttpRequest* req, HttpResponse* resp);
    int setPower(HttpRequest* req, HttpResponse* resp);
    int getDeviceList(HttpRequest* req, HttpResponse* resp);
    int getDeviceInfo(HttpRequest* req, HttpResponse* resp);
    int getModelInfo(HttpRequest* req, HttpResponse* resp);
};

#endif // _API_DEVICE_H_
