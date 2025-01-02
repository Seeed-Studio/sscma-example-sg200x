#ifndef _API_DAEMON_H_
#define _API_DAEMON_H_

#include "logger.hpp"

#include "hv/hlog.h"
#include "hv/hv.h"
#include "hv/mqtt_client.h"
#include "hv/requests.h"

typedef enum {
    APP_STATUS_NORMAL,
    APP_STATUS_STARTING,
    APP_STATUS_FAILED,
    APP_STATUS_NORESPONSE
} app_status_t;

#define MQTT_TOPIC_IN "sscma/v0/recamera/node/in/"
#define MQTT_TOPIC_OUT "sscma/v0/recamera/node/out/"
#define MQTT_PAYLOAD "{\"name\":\"health\",\"type\":3,\"data\":\"\"}"

class app_daemon {
public:
    app_daemon()
        : daemon_loop_exit(false)
        , sscma_status(APP_STATUS_NORMAL)
        , nodered_status(APP_STATUS_NORMAL)
    {
        hlog_disable();

        if (worker_thread_.joinable()) {
            std::cerr << "app_daemon is already running.\n";
            return;
        }
        syslog(LOG_INFO, "app_daemon starting...");
        sem_init(&sem_sscma, 0, 0);
        worker_thread_ = std::thread(&app_daemon::daemon_loop, this);
    }

    ~app_daemon()
    {
        syslog(LOG_INFO, "app_daemon stopping...");
        daemon_loop_exit = true;
        if (worker_thread_.joinable()) {
            worker_thread_.join();
            syslog(LOG_INFO, "app_daemon stopped.");
        }
    }

    app_status_t latest_sscma_status() { return sscma_status; }
    app_status_t latest_nodered_status() { return nodered_status; }

private:
    const std::string service_sscma = "sscma-node";
    const std::string service_nodered = "node-red";

    std::atomic<bool> daemon_loop_exit;
    std::thread worker_thread_;
    sem_t sem_sscma;
    hv::MqttClient cli;

    app_status_t sscma_status;
    app_status_t nodered_status;
    int start_flow(bool start);
    app_status_t get_flow_status();
    app_status_t get_sscma_status();
    app_status_t get_nodered_status();
    void check_service(const std::string& service, app_status_t* latest, std::function<app_status_t()> get_status);
    void daemon_loop();

    void init_mqtt_cli()
    {
        cli.onConnect = [](hv::MqttClient* cli) {
            cli->subscribe(MQTT_TOPIC_OUT);
            cli->publish(MQTT_TOPIC_IN, MQTT_PAYLOAD);
        };

        cli.onMessage = [this](hv::MqttClient* cli, mqtt_message_t* msg) {
            sem_post(&this->sem_sscma);
        };

        cli.onClose = [](hv::MqttClient* cli) {
            syslog(LOG_INFO, "mqtt disconnected");
        };

        cli.setPingInterval(10);
        cli.setID("supervisor");
        cli.connect("localhost", 1883, 0);

        std::thread([this]() { this->cli.run(); }).detach();
    }
};

#endif // _API_DAEMON_H_
