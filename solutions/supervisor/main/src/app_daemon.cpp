#include <syslog.h>

#include "api_base.h"
#include "app_daemon.h"

#define _LOOP_COND(c) ((!daemon_loop_exit) && (c))

int app_daemon::start_flow(bool start)
{
    hv::Json data;
    http_headers headers;

    data["state"] = start ? "start" : "stop";
    headers["Content-Type"] = "application/json";
    auto resp = requests::post("localhost:1880/flows/state", data.dump(), headers);
    if (resp == NULL) {
        syslog(LOG_ERR, "stop flow failed");
        return -1;
    }

    return 0;
}

app_status_t app_daemon::get_flow_status()
{
    auto resp = requests::get("localhost:1880/flows/state");
    if (resp == NULL)
        return APP_STATUS_NORESPONSE;

    try {
        hv::Json data = hv::Json::parse(resp->body.begin(), resp->body.end());
        if (data["state"] == "stop") {
            return APP_STATUS_STOP;
        }
    } catch (const std::exception& e) {
        return APP_STATUS_NORESPONSE;
    }

    return APP_STATUS_NORMAL;
}

app_status_t app_daemon::get_sscma_status()
{
    if (!cli.isConnected()) {
        syslog(LOG_ERR, "mqtt is not connected\n");
        cli.reconnect();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    cli.publish(MQTT_TOPIC_IN, MQTT_PAYLOAD);

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_nsec += 100 * 1000000; // 100ms
    if (0 == sem_timedwait(&sem_sscma, &ts))
        return APP_STATUS_NORMAL;

    return APP_STATUS_NORESPONSE;
}

app_status_t app_daemon::get_nodered_status()
{
    auto resp = requests::get("localhost:1880");
    if (NULL != resp)
        return APP_STATUS_NORMAL;

    return APP_STATUS_NORESPONSE;
}

void app_daemon::check_service(const std::string& service, std::function<app_status_t()> get_status)
{
    app_status_t status = get_status();
    while (_LOOP_COND(APP_STATUS_NORMAL != status)) {
        start_flow(false);
        api_base api_base;
        api_base.system_service(service, "restart");

        for (int i = 0; _LOOP_COND(i < 100); i++) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            status = get_status();
            std::cout << "status:" << status << std::endl;
            if (APP_STATUS_NORMAL == status) {
                syslog(LOG_INFO, std::string(service + " restart success.").c_str());
                return;
            }
        }
        syslog(LOG_ERR, std::string(service + " restart failed.").c_str());
    }

    return;
}

void app_daemon::daemon_loop()
{
    syslog(LOG_INFO, "daemon_loop started.");
    init_mqtt_cli();

    while (_LOOP_COND(true)) {
        app_status_t status = get_flow_status();
        if ((status == APP_STATUS_NORMAL) || (status == APP_STATUS_STOP)) {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            continue;
        } else {
            syslog(LOG_ERR, "get_flow_status: APP_STATUS_NORESPONSE");
        }

        check_service(service_sscma, [this]() { return get_sscma_status(); });
        check_service(service_nodered, [this]() { return get_nodered_status(); });

        start_flow(true);
    }

    cli.disconnect();
    cli.stop();
    sem_destroy(&sem_sscma);
    syslog(LOG_INFO, "daemon_loop exited.");
}
