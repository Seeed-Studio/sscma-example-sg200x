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
int noderedStatus = 0;
int noderedStarting = 0;
APP_STATUS sscmaStatus = APP_STATUS_UNKNOWN;
sem_t semSscma;

hv::MqttClient cli;

void runMqtt() {
    cli.run();
}

void initMqtt() {
    sem_init(&semSscma, 0, 0);

    cli.onConnect = [](hv::MqttClient* cli) {
        cli->subscribe(MQTT_TOPIC_OUT);
        cli->publish(MQTT_TOPIC_IN, MQTT_PAYLOAD);
    };

    cli.onMessage = [](hv::MqttClient* cli, mqtt_message_t* msg) {
        sem_post(&semSscma);
    };

    cli.onClose = [](hv::MqttClient* cli) {
        syslog(LOG_INFO, "mqtt disconnected");
    };

    cli.setPingInterval(10);
    cli.setID("supervisor");
    cli.connect("localhost", 1883, 0);

    std::thread th(runMqtt);
    th.detach();
}

int startFlow() {
    hv::Json data;
    http_headers headers;

    data["state"] = "start";
    headers["Content-Type"] = "application/json";

    auto resp = requests::post("localhost:1880/flows/state", data.dump(), headers);
    if (resp == NULL) {
        syslog(LOG_ERR, "stop flow failed");
        return -1;
    }

    return 0;
}

int stopFlow() {
    hv::Json data;
    http_headers headers;

    data["state"] = "stop";
    headers["Content-Type"] = "application/json";

    auto resp = requests::post("localhost:1880/flows/state", data.dump(), headers);
    if (resp == NULL) {
        syslog(LOG_ERR, "stop flow failed");
        return -1;
    }

    return 0;
}

int startApp(const char* cmd, const char* appName) {
    FILE* fp;
    char info[128] = "";

    fp = popen(cmd, "r");
    if (fp == NULL) {
        syslog(LOG_ERR, "Failed to run %s\n", cmd);
        return -1;
    }

    fgets(info, sizeof(info) - 1, fp);
    syslog(LOG_INFO, "%s status: %s\n", appName, info);

    pclose(fp);

    return 0;
}

int startNodered() {
    noderedStarting = 1;
    startApp(SCRIPT_DEVICE_RESTARTNODERED, "node-red");

    return 0;
}

int startSscma() {
    startApp(SCRIPT_DEVICE_RESTARTSSCMA, "sscma-node");

    return 0;
}

int getNoderedStatus() {
    for (int i = 0; i < retryTimes; i++) {
        auto resp = requests::get("localhost:1880");

        if (NULL != resp) {
            if (noderedStarting) {
                syslog(LOG_INFO, "node-red status: Finished\n");
            }
            noderedStarting = 0;
            noderedStatus = 1;
            return 0;
        }
    }

    return -1;
}

APP_STATUS getSscmaStatus() {
    struct timespec ts;

    if (!cli.isConnected()) {
        syslog(LOG_ERR, "mqtt is not connected\n");
        cli.reconnect();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    for (int i = 0; i < retryTimes; i++) {
        cli.publish(MQTT_TOPIC_IN, MQTT_PAYLOAD);

        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_nsec += 500 * 1000;
        if (0 == sem_timedwait(&semSscma, &ts)) {
            return APP_STATUS_NORMAL;
        }
    }

    return APP_STATUS_NORESPONSE;
}

void runDaemon() {
    initMqtt();

    while (daemonStatus) {
        std::this_thread::sleep_for(std::chrono::seconds(threadTimeout));
        APP_STATUS appStatus;

        if (0 != getNoderedStatus()) {
            if (noderedStarting) {
                syslog(LOG_INFO, "Nodered is starting");
            } else {
                if (noderedStatus) {
                    syslog(LOG_ERR, "Nodered is not responding");
                    startNodered();
                } else {
                    syslog(LOG_INFO, "Nodered has not yet fully started");
                }
            }
        }

        appStatus = getSscmaStatus();
        if (APP_STATUS_NORMAL == appStatus) {
            if (APP_STATUS_NORESPONSE == sscmaStatus) {
                syslog(LOG_INFO, "Restart Flow");
                startFlow();
            }
        } else {
            syslog(LOG_ERR, "sscma-node is not responding");
            stopFlow();
            startSscma();
        }
        sscmaStatus = appStatus;
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
