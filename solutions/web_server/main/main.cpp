#include <iostream>
#include <signal.h>
#include <string>
#include <syslog.h>
#include <unistd.h>

#include "http_server.h"
#include "logger.hpp"
#include "version.h"

#define ROOT_DIR "/usr/share/supervisor/www/"
#define HTTP_PORT "8000"

static int s_signum = 0;
void signal_handler(int signum)
{
    syslog(LOG_INFO, "Received signal %d", signum);
    signal(signum, signal_handler);
    s_signum = signum;
}

int main(int argc, char** argv)
{
    MA_LOG_INIT("web_server", LOG_PID | LOG_CONS, LOG_USER);

    if (argc > 1 && std::string(argv[1]) == "-v") {
        cout << "Build Time: " << __DATE__ << " " << __TIME__ << endl;
        cout << PROJECT_NAME << " V" << PROJECT_VERSION << endl;
    }

    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    http_server server(ROOT_DIR);
    if (!server.start(HTTP_PORT)) {
        MA_LOGE("Failed: server.start()\n");
        return 1;
    }

    while (s_signum == 0) {
        sleep(1);
    }

    MA_LOGV("exited\n");
    MA_LOG_DEINIT();

    return 0;
}
