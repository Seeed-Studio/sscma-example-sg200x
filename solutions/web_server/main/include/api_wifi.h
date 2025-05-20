#ifndef API_WIFI_H
#define API_WIFI_H

#include "api_base.h"

class api_wifi : public api_base {
private:
    static api_status_t autoConnectWiFi(request_t req, response_t res);
    static api_status_t connectWiFi(request_t req, response_t res);
    static api_status_t disconnectWiFi(request_t req, response_t res);
    static api_status_t forgetWiFi(request_t req, response_t res);
    static api_status_t getWiFiScanResults(request_t req, response_t res);
    static api_status_t getWifiStatus(request_t req, response_t res);
    static api_status_t queryWiFiInfo(request_t req, response_t res);
    static api_status_t scanWiFi(request_t req, response_t res);
    static api_status_t switchWiFi(request_t req, response_t res);

public:
    api_wifi()
        : api_base("wifiMgr")
    {
        MA_LOGV("");

        REG_API(autoConnectWiFi);
        REG_API(connectWiFi);
        REG_API(disconnectWiFi);
        REG_API(forgetWiFi);
        REG_API(getWiFiScanResults);
        REG_API(getWifiStatus);
        REG_API(queryWiFiInfo);
        REG_API(scanWiFi);
        REG_API(switchWiFi);
    }

    ~api_wifi()
    {
        MA_LOGV("");
    }
};

#endif // API_WIFI_H
