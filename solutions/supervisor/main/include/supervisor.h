#ifndef _SUPERVISOR_H_
#define _SUPERVISOR_H_

#include "api_device.h"
#include "api_update.h"
#include "app_daemon.h"
#include "http_server.h"

#undef TAG
#define TAG "supervisor"

class http_server;
class app_daemon;
class api_device;

class supervisor {
public:
    supervisor()
        : server(std::make_shared<http_server>(resource_dir, redirect_url))
        , daemon(std::make_shared<app_daemon>())
        , device(std::make_shared<api_device>())
        , update(std::make_shared<api_update>())
    {
        MA_LOGI(TAG, "*****start*****");

        bind_callbacks();
    }

    ~supervisor()
    {
        MA_LOGI(TAG, "*****exit*****");
    }

    void start()
    {
        MA_LOGI(TAG, "%s", __func__);
        server->register_apis(device->get_apis());
        server->start();
    }

private:
    const std::string resource_dir = "/usr/share/supervisor/www";
    const std::string redirect_url = "http://192.168.16.1/index.html";
    std::shared_ptr<http_server> server;
    std::shared_ptr<app_daemon> daemon;
    std::shared_ptr<api_device> device;
    std::shared_ptr<api_update> update;

    void bind_callbacks()
    {
        device->sscma_status = std::bind(&app_daemon::latest_sscma_status, daemon.get());
        device->nodered_status = std::bind(&app_daemon::latest_nodered_status, daemon.get());
    }
};

#endif // _SUPERVISOR_H_
