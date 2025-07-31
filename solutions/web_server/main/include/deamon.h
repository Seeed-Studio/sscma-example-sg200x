#ifndef __DEAMON_H__
#define __DEAMON_H__

#include <atomic>
#include <condition_variable>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

#include "api_base.h"
#include "logger.hpp"

class deamon {
    typedef enum {
        STATUS_NORMAL,
        STATUS_STARTING,
        STATUS_FAILED,
        STATUS_NORESPONSE,
        STATUS_UNKOWN,

        STATUS_MAX
    } status_t;

public:
    deamon()
    {
        LOGV("");
        thread_ = std::thread([this]() {
            uint32_t wait_sscma = 0;
            uint32_t wait_nodered = 0;
            running_ = true;
            while (running_) {
                std::this_thread::sleep_for(std::chrono::seconds(3));
                query_sscma();
                if (sscma_status_ != STATUS_NORMAL) {
                    if (0 == wait_sscma)
                        start_service("sscma");
                    if (wait_sscma++ > 10) {
                        wait_sscma = 0;
                    }
                    continue;
                }
                wait_sscma = 0;

                query_flow();
                query_nodered();
                if (nodered_status_ != STATUS_NORMAL) {
                    if (0 == wait_nodered)
                        start_service("nodered");
                    if (wait_nodered++ > 40) {
                        wait_nodered = 0;
                    }
                    continue;
                }
                wait_nodered = 0;
            }
        });
        thread_.detach();
    }

    ~deamon()
    {
        running_ = false;
        if (thread_.joinable()) {
            thread_.join();
        }
        LOGV("");
    }

    status_t get_sscma_status() { return sscma_status_; }
    status_t get_nodered_status() { return nodered_status_; }
    status_t get_flow_status() { return flow_status_; }

private:
    std::thread thread_;
    std::atomic<bool> running_;

    status_t sscma_status_ = STATUS_UNKOWN;
    status_t nodered_status_ = STATUS_UNKOWN;
    status_t flow_status_ = STATUS_UNKOWN;

    void query_sscma()
    {
        std::string result = api_base::script(__func__);
        if (result.empty()) {
            sscma_status_ = STATUS_FAILED;
            return;
        }
        sscma_status_ = (result == "Timed out") ? STATUS_NORESPONSE : STATUS_NORMAL;
    }

    void query_flow()
    {
        flow_status_ = STATUS_NORMAL;
        std::string result = api_base::script(__func__);
        if (result.empty() || result == "Failed") {
            flow_status_ = STATUS_FAILED;
            return;
        }
    }

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
        std::string result = api_base::script(__func__, service);
        if (result.empty() || result != "OK") {
            LOGV("start service %s failed", service.c_str());
            return;
        }
    }
};

#endif // __DEAMON_H__
