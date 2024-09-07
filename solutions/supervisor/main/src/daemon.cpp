#include <stdio.h>
#include <string.h>
#include <syslog.h>

#include "hv/requests.h"
#include "global_cfg.h"
#include "daemon.h"

const int threadTimeout = 3;
int daemonStatus = 0;
int noderedStarting = 0;

int startNodered() {
    char cmd[128] = SCRIPT_DEVICE_RESTARTAPP;

    noderedStarting = 1;
    strcat(cmd, "node-red");
    system(cmd);

    return 0;
}

int startSscma() {
    // TODO

    return 0;
}

int getNoderedStatus() {
    auto resp = requests::get("localhost:1880");

    if (resp == NULL) {
        return -1;
    }

    noderedStarting = 0;

    return 0;
}

int getSscmaStatus() {
    return 0;
}

void runDaemon() {
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
    std::thread th(runDaemon);

    daemonStatus = 1;
    th.detach();
}

void stopDaemon() {
    daemonStatus = 0;
}
