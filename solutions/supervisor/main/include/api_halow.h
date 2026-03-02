#ifndef API_HALOW_H
#define API_HALOW_H

#include "../../components/mongoose/json.hpp"
#include <atomic>
#include <condition_variable>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

#include "api_base.h"
#include "logger.hpp"

class api_halow : public api_base {
private:
    // APIs
    static api_status_t connectHalow(request_t req, response_t res);
    static api_status_t disconnectHalow(request_t req, response_t res);
    static api_status_t forgetHalow(request_t req, response_t res);
    static api_status_t getHalowInfoList(request_t req, response_t res);
    static api_status_t switchHalow(request_t req, response_t res);
    static api_status_t switchAntenna(request_t req, response_t res);
    static api_status_t startPing(request_t req, response_t res);
    static api_status_t stopPing(request_t req, response_t res);
    static api_status_t getPingStatus(request_t req, response_t res);

public:
    api_halow()
        : api_base("halowMgr")
    {
        LOGV("");
        REG_API(connectHalow);
        REG_API(disconnectHalow);
        REG_API(forgetHalow);
        REG_API(getHalowInfoList);
        REG_API(switchHalow);
        REG_API(switchAntenna);
        REG_API(startPing);
        REG_API(stopPing);
        REG_API(getPingStatus);

        start_halow();
    }

    ~api_halow()
    {
        stop_ping();
        stop_halow();
        LOGV("");
    }

private:
    static inline int _sta_enable = 1;
    static inline int _antennaMode = 1;
    static inline json _nw_info;
    static inline int8_t _failed_cnt = 10;

    // Ping task
    static inline std::thread _ping_worker;
    static inline std::atomic<bool> _ping_running { false };
    static inline std::atomic<int> _ping_interval { 3 };
    static inline std::string _ping_ip;
    static inline std::condition_variable _ping_cv;
    static inline std::mutex _ping_mutex;
    
    static void start_ping(const std::string& ip, int interval);
    static void stop_ping();

    // thread
    std::thread _worker;
    static inline std::atomic<bool> _running { true };
    static inline std::condition_variable _cv;
    static inline std::mutex _halow_mutex;
    static inline bool _need_scan;
    static void trigger_scan()
    {
        _need_scan = true;
        _cv.notify_one();
    }

    json get_eth();
    json get_halow_current();
    json get_halow_connected(json& current);
    json get_halow_scan_list(json& connected);

    void start_halow();
    void stop_halow();
    static api_status_t _halow_ctrl(request_t req, response_t res, std::string ctrl);
};

#endif // API_HALOW_H
