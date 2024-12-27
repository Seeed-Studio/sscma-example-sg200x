#include <iostream>
#include <signal.h>
#include <string>
#include <syslog.h>
#include <thread>
#include <unistd.h>

#include "app_daemon.h"
#include "http_server.h"
#include "device.h"

#define RESOURCE_DIR "/usr/share/supervisor/www/"
#define REDIRECT_URL "http://192.168.16.1/index.html"

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
    openlog("supervisor", LOG_CONS | LOG_PID, 0);
    setlogmask(LOG_UPTO(LOG_DEBUG));

    signal(SIGINT, &exitHandle);
    signal(SIGTERM, &exitHandle);

    std::cout << "*****start*****" << std::endl;

    {
        app_daemon app_daemon;

        http_server http_server(RESOURCE_DIR);
        http_server.register_redirect(REDIRECT_URL);
        http_server.start();

        main_loop_exit = false;
        while (!main_loop_exit) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        std::cout << "*****end*****" << std::endl;
    }

    std::cout << "*****exit*****" << std::endl;
    closelog();

    return 0;
}
