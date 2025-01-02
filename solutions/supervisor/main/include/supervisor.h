#ifndef _SUPERVISOR_H_
#define _SUPERVISOR_H_

#include "api_device.h"
#include "app_daemon.h"
#include "http_server.h"

#undef TAG
#define TAG "supervisor"

class http_server;
class app_daemon;
class api_device;

class supervisor {
private:
    const std::string resource_dir = "/usr/share/supervisor/www";
    const std::string redirect_url = "http://192.168.16.1/index.html";
    std::shared_ptr<http_server> server;

public:
    std::shared_ptr<app_daemon> daemon;
    std::shared_ptr<api_device> device;

    supervisor()
        : server(std::make_shared<http_server>(resource_dir, redirect_url))
        , daemon(std::make_shared<app_daemon>())
        , device(std::make_shared<api_device>(this))
    {
        MA_LOG_INIT("supervisor", LOG_CONS | LOG_PID, 0);
        MA_LOG_MASK(LOG_UPTO(LOG_DEBUG));
        MA_LOGD(TAG, "*****start*****");
    }

    ~supervisor()
    {
        MA_LOGD(TAG, "*****exit*****");
        MA_LOG_DEINIT();
    }

    void start()
    {
        MA_LOGE(TAG, "%s", __func__);
        MA_LOGD(TAG, "%s", __func__);
        server->register_apis(device->get_apis());
        server->start();
    }
};

#endif
