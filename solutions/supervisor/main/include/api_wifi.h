#ifndef API_WIFI_H
#define API_WIFI_H

#include "../../components/mongoose/json.hpp"
#include <atomic>
#include <condition_variable>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

#include "api_base.h"
#include "logger.hpp"

class api_wifi : public api_base {
private:
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
        // stop_wifi();
        LOGV("");
    }

private:
    static inline int _sta_enable = 1;
    static inline int _ap_enable = 1;

    std::thread _worker;
    static inline std::atomic<bool> _running { true };
    static inline std::condition_variable cv;
    static inline std::mutex wifi_mutex;

    // 0: not connected, 1: connected
    static inline std::atomic<int> _eth_status { 0 };
    // 0: disabled, 1: not connected, 2: connecting, 3: connected, 4: not supported
    static inline std::atomic<int> _sta_status { 0 };

    static inline json _eth;
    static inline json _sta_current;
    static inline json _sta_connected;

    static json get_eth();
    static json get_sta_current();
    static json get_sta_connected();
    void start_wifi();
    void stop_wifi();

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
};

#endif // API_WIFI_H
