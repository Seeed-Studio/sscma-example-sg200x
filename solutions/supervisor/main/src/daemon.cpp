#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <semaphore.h>
#include <time.h>

#include "hv/mqtt_client.h"
#include "hv/hv.h"
#include "hv/requests.h"
#include "global_cfg.h"
#include "daemon.h"

#define MQTT_TOPIC_IN  "sscma/v0/recamera/node/in/"
#define MQTT_TOPIC_OUT "sscma/v0/recamera/node/out/"
#define MQTT_PAYLOAD   "{\"name\":\"health\",\"type\":3,\"data\":\"\"}"

const int threadTimeout = 3;
const int retryTimes = 3;
int daemonStatus = 0;
int noderedStarting = 0;
sem_t semSscma;

hv::MqttClient cli;

void runMqtt() {
    cli.run();
}

void initMqtt() {
    sem_init(&semSscma, 0, 0);

    cli.onConnect = [](hv::MqttClient* cli) {
        cli->subscribe(MQTT_TOPIC_OUT);
    };

    cli.onMessage = [](hv::MqttClient* cli, mqtt_message_t* msg) {
        sem_post(&semSscma);
    };

    cli.onClose = [](hv::MqttClient* cli) {
        syslog(LOG_INFO, "mqtt disconnected");
    };

    cli.setPingInterval(10);
    cli.connect("localhost", 1883, 0);

    std::thread th(runMqtt);
    th.detach();
}

int startNodered() {
    char cmd[128] = SCRIPT_DEVICE_RESTARTAPP;

    noderedStarting = 1;
    strcat(cmd, "node-red");
    system(cmd);

    return 0;
}

int startSscma() {
    system(SCRIPT_APP_SSCMA_RESTART);

    return 0;
}

int getNoderedStatus() {
    for (int i = 0; i < retryTimes; i++) {
        auto resp = requests::get("localhost:1880");

        if (NULL != resp) {
            noderedStarting = 0;
            return 0;
        }
    }

    return -1;
}

int getSscmaStatus() {
    struct timespec ts;

    for (int i = 0; i < retryTimes; i++) {
        cli.publish(MQTT_TOPIC_IN, MQTT_PAYLOAD);

        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_nsec += 500 * 1000;
        if (0 == sem_timedwait(&semSscma, &ts)) {
            return 0;
        }
    }

    return -1;
}

void runDaemon() {
    initMqtt();

    while (daemonStatus) {
        if (0 != getNoderedStatus()) {
            if (noderedStarting) {
                syslog(LOG_INFO, "Nodered is starting");
            } else {
                syslog(LOG_ERR, "Nodered is not responding");
                startNodered();
            }
        }

        if (0 != getSscmaStatus()) {
            syslog(LOG_ERR, "Sscma is not responding");
            startSscma();
        }

        std::this_thread::sleep_for(std::chrono::seconds(threadTimeout));
    }
}

void initDaemon() {
    std::thread th;

    daemonStatus = 1;
    th = std::thread(runDaemon);
    th.detach();
}

void stopDaemon() {
    daemonStatus = 0;
}
