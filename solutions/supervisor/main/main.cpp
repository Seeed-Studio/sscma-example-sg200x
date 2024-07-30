#include <iostream>
#include <string>
#include <unistd.h>
#include <signal.h>

#include "http.h"

static void exitHandle(int signo) {
    system(SCRIPT_WIFI_STOP);
    deinitHttpd();

    exit(0);
}

static void initSupervisor() {
    system(SCRIPT_WIFI_START);
    initHttpd();

    signal(SIGINT, &exitHandle);
    signal(SIGTERM, &exitHandle);
}

int main(int argc, char** argv) {
    initSupervisor();

    while(1) sleep(1000);

    system(SCRIPT_WIFI_STOP);
    deinitHttpd();

    return 0;
}
