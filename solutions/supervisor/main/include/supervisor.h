#ifndef _SUPERVISOR_H_
#define _SUPERVISOR_H_

#include "api_device.h"
#include "app_daemon.h"
#include "http_server.h"

class http_server;
class app_daemon;
class api_device;

class supervisor {
private:
    const std::string resource_dir = "/usr/share/supervisor/www";
    const std::string redirect_url = "http://192.168.16.1/index.html";

    std::shared_ptr<http_server> server;
    std::shared_ptr<app_daemon> daemon;
    std::shared_ptr<api_device> device;

public:
    supervisor()
        : server(std::make_shared<http_server>(resource_dir, redirect_url))
        , daemon(std::make_shared<app_daemon>())
        , device(std::make_shared<api_device>())
    {
        openlog("supervisor", LOG_CONS | LOG_PID, 0);
        setlogmask(LOG_UPTO(LOG_DEBUG));
        std::cout << "*****start*****" << std::endl;
    }

    ~supervisor()
    {
        std::cout << "*****exit*****" << std::endl;
        closelog();
    }

    void start()
    {
        server->register_apis(device->get_apis());
        server->start();
    }
};

#endif
