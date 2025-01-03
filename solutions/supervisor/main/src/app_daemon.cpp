#include "api_base.h"

#undef TAG
#define TAG "app_daemon"

#define _LOOP_COND(c) ((!daemon_loop_exit) && (c))

int app_daemon::start_flow(bool start)
{
    hv::Json data;
    http_headers headers;

    data["state"] = start ? "start" : "stop";
    headers["Content-Type"] = "application/json";
    auto resp = requests::post("localhost:1880/flows/state", data.dump(), headers);
    if (resp == NULL) {
        MA_LOGE(TAG, "%s flow failed", data["state"].dump());
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
        // if (data["state"] == "stop") {
        //     return APP_STATUS_STOP;
        // }
    } catch (const std::exception& e) {
        return APP_STATUS_NORESPONSE;
    }

    return APP_STATUS_NORMAL;
}

app_status_t app_daemon::get_sscma_status()
{
    if (!cli.isConnected()) {
        MA_LOGI(TAG, "mqtt is not connected");
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
    return (NULL != resp) ? APP_STATUS_NORMAL : APP_STATUS_NORESPONSE;
}

void app_daemon::check_service(const std::string& service, app_status_t* latest, std::function<app_status_t()> get_status)
{
    app_status_t status = get_status();
    while (_LOOP_COND(APP_STATUS_NORMAL != status)) {
        start_flow(false);
        api_base api_base;
        api_base.system_service(service, "restart");

        *latest = APP_STATUS_STARTING;
        for (int i = 0; _LOOP_COND(i < 100); i++) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            status = get_status();
            MA_LOGD(TAG, "%s, status:%d", service.c_str(), status);
            if (APP_STATUS_NORMAL == status) {
                *latest = APP_STATUS_NORMAL;
                MA_LOGI(TAG, "%s restart success.", service.c_str());
                return;
            }
        }
        *latest = APP_STATUS_FAILED;
        MA_LOGI(TAG, "%s restart failed.", service.c_str());
    }
    *latest = status;

    return;
}

void app_daemon::daemon_loop()
{
    MA_LOGI(TAG, "daemon_loop started.");
    init_mqtt_cli();

    while (_LOOP_COND(true)) {
        app_status_t status = get_flow_status();
        if ((status == APP_STATUS_NORMAL) /*|| (status == APP_STATUS_STOP)*/) {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            continue;
        } else {
            MA_LOGE(TAG, "get_flow_status: APP_STATUS_NORESPONSE");
        }

        check_service(service_sscma, &sscma_status, [this]() { return get_sscma_status(); });
        check_service(service_nodered, &nodered_status, [this]() { return get_nodered_status(); });

        start_flow(true);
    }

    cli.disconnect();
    cli.stop();
    sem_destroy(&sem_sscma);
    MA_LOGI(TAG, "daemon_loop exited.");
}
