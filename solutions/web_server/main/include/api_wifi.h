#ifndef API_WIFI_H
#define API_WIFI_H

#include "api_base.h"

class api_wifi : public api_base {
private:
    static api_status_t autoConnectWiFi(const json& request, json& response);
    static api_status_t connectWiFi(const json& request, json& response);
    static api_status_t disconnectWiFi(const json& request, json& response);
    static api_status_t forgetWiFi(const json& request, json& response);
    static api_status_t getWiFiScanResults(const json& request, json& response);
    static api_status_t getWifiStatus(const json& request, json& response);
    static api_status_t queryWiFiInfo(const json& request, json& response);
    static api_status_t scanWiFi(const json& request, json& response);
    static api_status_t switchWiFi(const json& request, json& response);

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
