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
            uint8_t wait_nodered = 0;
            bool restart_flow = false;
            running_ = true;
            while (running_) {
                std::this_thread::sleep_for(std::chrono::seconds(10));
                query_nodered();
                LOGD("nodered_status_=%d", nodered_status_);
                if (nodered_status_ != STATUS_NORMAL) {
                    LOGW("nodered_status_=%d, wait_nodered=%d", nodered_status_, wait_nodered);
                    if (3 == wait_nodered)
                        start_service("nodered");
                    if (wait_nodered++ > 15) {
                        wait_nodered = 0;
                    }
                    continue;
                }
                wait_nodered = 0;

                query_sscma();
                if (sscma_status_ != STATUS_NORMAL) {
                    start_service("sscma");
                    restart_flow = true;
                    continue;
                }

                if (restart_flow) {
                    api_base::script("ctrl_flow", "stop");
                    api_base::script("ctrl_flow", "start");
                    restart_flow = false;
                }
            }
        });
        thread_.detach();
    }

    ~serviced()
    {
        running_ = false;
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
