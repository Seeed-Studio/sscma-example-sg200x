#include <iostream>
#include <signal.h>
#include <string>
#include <syslog.h>
#include <thread>
#include <unistd.h>

#include "supervisor.h"

static std::atomic<bool> main_loop_exit;

static void exitHandle(int signo)
{
    syslog(LOG_INFO, "received signal %d", signo);
    std::cout << "received signal " << signo << std::endl;

    main_loop_exit = true;
    // exit(0);
}

int main(int argc, char** argv)
{
    signal(SIGINT, &exitHandle);
    signal(SIGTERM, &exitHandle);

    auto ptr_app = new supervisor;
    ptr_app->start();

    main_loop_exit = false;
    while (!main_loop_exit) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    std::cout << "*****end*****" << std::endl;

    delete ptr_app;

    return 0;
}
