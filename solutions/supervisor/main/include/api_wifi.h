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
    static api_status_t autoConnectWiFi(request_t req, response_t res);
    static api_status_t connectWiFi(request_t req, response_t res);
    static api_status_t disconnectWiFi(request_t req, response_t res);
    static api_status_t forgetWiFi(request_t req, response_t res);
    static api_status_t getWiFiInfoList(request_t req, response_t res);
    static api_status_t switchWiFi(request_t req, response_t res);

public:
    api_wifi()
        : api_base("wifiMgr")
    {
        LOGV("");
        REG_API(autoConnectWiFi);
        REG_API(connectWiFi);
        REG_API(disconnectWiFi);
        REG_API(forgetWiFi);
        REG_API(getWiFiInfoList);
        REG_API(switchWiFi);

        start_wifi();
    }

    ~api_wifi()
    {
        stop_wifi();
        LOGV("");
    }

private:
    static inline int _sta_enable = 1;
    static inline int _ap_enable = 1;
    static inline json _nw_info;

    // thread
    std::thread _worker;
    static inline std::atomic<bool> _running { true };
    static inline std::condition_variable cv;
    static inline std::mutex wifi_mutex;
    static inline uint8_t _need_scan = 1;
 
    json get_eth();
    json get_sta_current();
    json get_sta_connected(json& current);
    json get_scan_list(json& current, json& connected);

    void start_wifi();
    void stop_wifi();
    static api_status_t _wifi_ctrl(request_t req, response_t res, std::string ctrl);
};

#endif // API_WIFI_H
