#ifndef SERVICED_H
#define SERVICED_H

#include <atomic>
#include <condition_variable>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

#include "api_base.h"
#include "logger.hpp"

class serviced {
    typedef enum {
        STATUS_NORMAL,
        STATUS_STARTING,
        STATUS_FAILED,
        STATUS_NORESPONSE,
        STATUS_UNKOWN,

        STATUS_MAX
    } status_t;

public:
    serviced()
    {
        LOGV("");
        thread_ = std::thread([this]() {
            bool sys_booting = true;
            uint8_t wait_nodered = 0;
            bool restart_flow = false;

            running_ = true;
            while (running_) {
                std::unique_lock<std::mutex> lock(mutex_);
                if (cv_.wait_for(lock, std::chrono::seconds(6), [this] { return !running_; })) {
                    break;
                }
                query_nodered();
                query_sscma();
                LOGV("nodered_status_=%d, sscma_status_=%d", nodered_status_, sscma_status_);

                if (sys_booting) {
                    uint64_t t = api_base::uptime();
                    LOGD("uptime: %llu", t);
                    if (t >= 2 * 60 * 1000) // 2 minutes
                        sys_booting = false;
                }
                if (sys_booting)
                    continue;

                if (nodered_status_ != STATUS_NORMAL) {
                    LOGV("wait_nodered=%d", wait_nodered);
                    if (3 == wait_nodered) { // Continuous failure
                        start_service("nodered");
                    }
                    if (wait_nodered++ > 20) { // Timeout, will start again
                        wait_nodered = 0;
                    }
                    continue;
                }
                wait_nodered = 0;

                if (sscma_status_ != STATUS_NORMAL) {
                    start_service("sscma");
                    restart_flow = true;
                    continue; // To check sscma ready
                }
                if (restart_flow) {
                    LOGW("Restart flow");
                    api_base::script("ctrl_flow", "stop");
                    api_base::script("ctrl_flow", "start");
                    restart_flow = false;
                }
            }
            LOGV("serviced thread exit");
        });
    }

    ~serviced()
    {
        running_ = false;
        cv_.notify_all();
        if (thread_.joinable()) {
            thread_.join();
        }
        LOGV("");
    }

    status_t get_sscma_status() { return sscma_status_; }
    status_t get_nodered_status() { return nodered_status_; }
    // status_t get_flow_status() { return flow_status_; }

private:
    std::thread thread_;
    std::atomic<bool> running_;
    std::mutex mutex_;
    std::condition_variable cv_;

    status_t sscma_status_ = STATUS_UNKOWN;
    status_t nodered_status_ = STATUS_UNKOWN;
    // status_t flow_status_ = STATUS_UNKOWN;

    void query_sscma()
    {
        std::string result = api_base::script(__func__);
        if (result.empty() || (result == "Timed out") || (result == "Failed")) {
            sscma_status_ = STATUS_FAILED;
            return;
        }
        sscma_status_ = STATUS_NORMAL;
    }

    // void query_flow()
    // {
    //     flow_status_ = STATUS_NORMAL;
    //     std::string result = api_base::script(__func__);
    //     if (result.empty() || result == "Failed") {
    //         flow_status_ = STATUS_FAILED;
    //         return;
    //     }
    // }

    void query_nodered()
    {
        nodered_status_ = STATUS_NORMAL;
        std::string result = api_base::script(__func__);
        if (result.empty() || result != "OK") {
            nodered_status_ = STATUS_FAILED;
            return;
        }
    }

    void start_service(const std::string& service)
    {
        LOGW("start service %s", service.c_str());
        std::string result = api_base::script(__func__, service);
        if (result.empty() || result != "OK") {
            LOGE("start service %s failed", service.c_str());
            return;
        }
    }
};

#endif // SERVICED_H
