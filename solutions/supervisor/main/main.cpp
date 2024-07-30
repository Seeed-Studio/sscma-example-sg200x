#include <iostream>
#include <string>
#include <unistd.h>
#include <signal.h>

#include "http.h"

static void exitHandle(int signo) {
    system(SCRIPT_WIFI_STOP);
    app_ipc_Httpd_DeInit();

    exit(0);
}

int main(int argc, char** argv) {
    system(SCRIPT_WIFI_START);
    app_ipc_Httpd_Init();

    signal(SIGINT, &exitHandle);
    signal(SIGTERM, &exitHandle);

    while(1) sleep(1000);

    system(SCRIPT_WIFI_STOP);
    app_ipc_Httpd_DeInit();

    return 0;
}
