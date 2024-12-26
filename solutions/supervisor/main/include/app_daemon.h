#ifndef _DAEMON_H_
#define _DAEMON_H_

#include "hv/hv.h"
#include "hv/mqtt_client.h"
#include "hv/requests.h"

#define MQTT_TOPIC_IN "sscma/v0/recamera/node/in/"
#define MQTT_TOPIC_OUT "sscma/v0/recamera/node/out/"
#define MQTT_PAYLOAD "{\"name\":\"health\",\"type\":3,\"data\":\"\"}"

typedef enum {
    APP_STATUS_NORMAL,
    APP_STATUS_STOP,
    APP_STATUS_NORESPONSE,
    APP_STATUS_STARTFAILED,
    APP_STATUS_UNKNOWN
} app_status_t;

class app_daemon {
public:
    app_daemon()
    {
        daemon_loop_exit = false;
        sem_init(&sem_sscma, 0, 0);
        std::thread th(&app_daemon::daemon_loop, this);
        th.detach();

        syslog(LOG_INFO, "Daemon started.");
        std::cout << "Daemon started." << std::endl;
    }

    ~app_daemon()
    {
        daemon_loop_exit = true;
        syslog(LOG_INFO, "Stopping daemon...");
        std::cout << "Stopping daemon..." << std::endl;
    }

    app_status_t query_nodered_status() { return nodered_status; };
    app_status_t query_sscma_status() { return sscma_status; }
    app_status_t query_flow_status() { return flow_status; }

    int restart_nodered();
    int restart_sscma();

private:
    std::atomic<bool> daemon_loop_exit;
    sem_t sem_sscma;
    hv::MqttClient cli;

    app_status_t nodered_status;
    app_status_t sscma_status;
    app_status_t flow_status;

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

        std::thread th([this]() {
            this->cli.run();
        });
        th.detach();
    }

    // int start_service(std::string path);
    int start_flow(bool start);
    app_status_t get_flow_status();
    app_status_t get_nodered_status();
    app_status_t get_sscma_status();
    void daemon_loop();
};

// typedef enum {
//     APP_STATUS_NORMAL,
//     APP_STATUS_STOP,
//     APP_STATUS_NORESPONSE,
//     APP_STATUS_STARTFAILED,
//     APP_STATUS_UNKNOWN
// } ;

// extern int noderedStarting;
// extern int sscmaStarting;
// extern APP_STATUS noderedStatus;
// extern APP_STATUS sscmaStatus;

// int stopFlow();

// void initDaemon();
// void stopDaemon();

#endif
