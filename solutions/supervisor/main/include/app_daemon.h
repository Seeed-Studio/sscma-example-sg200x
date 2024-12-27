#ifndef _DAEMON_H_
#define _DAEMON_H_

#include "hv/hv.h"
#include "hv/mqtt_client.h"
#include "hv/requests.h"

typedef enum {
    APP_STATUS_NORMAL,
    APP_STATUS_STOP,
    APP_STATUS_NORESPONSE,
    APP_STATUS_STARTFAILED,
    APP_STATUS_UNKNOWN
} app_status_t;

#define MQTT_TOPIC_IN "sscma/v0/recamera/node/in/"
#define MQTT_TOPIC_OUT "sscma/v0/recamera/node/out/"
#define MQTT_PAYLOAD "{\"name\":\"health\",\"type\":3,\"data\":\"\"}"

extern std::string exec_shell_cmd(const std::string& cmd);
class app_daemon {
public:
    app_daemon()
        : daemon_loop_exit(false)
    {
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

    app_status_t get_flow_status();
    app_status_t get_nodered_status();
    app_status_t get_sscma_status();

private:
    const std::string service_sscma = "sscma-node";
    const std::string service_nodered = "node-red";

    std::atomic<bool> daemon_loop_exit;
    std::thread worker_thread_;
    sem_t sem_sscma;
    hv::MqttClient cli;

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

    void check_service(std::string service, std::function<app_status_t()> get_status);
    int start_flow(bool start);
    void daemon_loop();
};

#endif
