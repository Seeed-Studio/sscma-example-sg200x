#include <iostream>
#include <string>
#include <unistd.h>
#include <signal.h>
#include <syslog.h>

#include "hv/hlog.h"
#include "http.h"
#include "daemon.h"

static void exitHandle(int signo) {
    stopDaemon();
    deinitHttpd();
    stopWifi();
    closelog();

    exit(0);
}

static void initSupervisor() {
    hlog_disable();
    openlog("supervisor", LOG_CONS | LOG_PERROR, LOG_DAEMON);

    initWiFi();
    initHttpd();
    initDaemon();

    signal(SIGINT, &exitHandle);
    signal(SIGTERM, &exitHandle);

    setlogmask(LOG_UPTO(LOG_NOTICE));
}

int main(int argc, char** argv) {

    printf("Build Time: %s %s\n", __DATE__, __TIME__);

    initSupervisor();

    while(1) sleep(1000);

    system(SCRIPT_WIFI_STOP);
    deinitHttpd();

    closelog();

    return 0;
}
