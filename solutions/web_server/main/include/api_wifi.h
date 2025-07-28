#ifndef API_WIFI_H
#define API_WIFI_H

#include "api_base.h"
#include "logger.hpp"
#include <map>
#include <vector>

class api_wifi : public api_base {
private:
    static inline int _is_open = 1;

    static json get_sta_connected();
    static json get_sta_current();
    static json get_eth();

    static api_status_t _wifi_ctrl(request_t req, response_t res, std::string ctrl)
    {
        auto&& body = parse_body(req);
        std::string ssid = body.value("ssid", "");
        if (ssid.empty()) {
            response(res, -1, STR_FAILED);
            return API_STATUS_OK;
        }

        auto result = script(ctrl, ssid);
        response(res, result == STR_OK ? 0 : -1, result);
        return API_STATUS_OK;
    }

    // APIs
    static api_status_t disconnectWiFi(request_t req, response_t res)
    {
        return _wifi_ctrl(req, res, __func__);
    }
    static api_status_t forgetWiFi(request_t req, response_t res)
    {
        return _wifi_ctrl(req, res, __func__);
    }
    static api_status_t autoConnectWiFi(request_t req, response_t res)
    {
        response(res);
        return API_STATUS_OK;
    }
    static api_status_t connectWiFi(request_t req, response_t res);
    static api_status_t scanWiFi(request_t req, response_t res);
    static api_status_t getWiFiScanResults(request_t req, response_t res);
    static api_status_t getWifiStatus(request_t req, response_t res);
    static api_status_t queryWiFiInfo(request_t req, response_t res);
    static api_status_t switchWiFi(request_t req, response_t res);

    static void start_wifi()
    {
        auto result = script(__func__);
        if (result.compare("-1") == 0) {
            _is_open = -1;
        } else if (result.compare("0") == 0) {
            _is_open = 0;
        } else {
            _is_open = 1;
        }
    }

    static void stop_wifi()
    {
        script(__func__);
    }

public:
    api_wifi()
        : api_base("wifiMgr")
    {
        LOGV("");
        REG_API(connectWiFi);
        REG_API(disconnectWiFi);
        REG_API(forgetWiFi);
        REG_API(autoConnectWiFi);
        REG_API(scanWiFi);
        REG_API(getWiFiScanResults);
        REG_API(getWifiStatus);
        REG_API(queryWiFiInfo);
        REG_API(switchWiFi);

        start_wifi();
    }

    ~api_wifi()
    {
        stop_wifi();
        LOGV("");
    }
};

#endif // API_WIFI_H
