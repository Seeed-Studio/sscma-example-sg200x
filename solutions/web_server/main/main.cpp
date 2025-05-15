#include <iostream>
#include <signal.h>
#include <string>
#include <syslog.h>
#include <unistd.h>

#include "http_server.h"
#include "version.h"

#define ROOT_DIR "/usr/share/supervisor/www/"
#define HTTP_PORT ":8000"

static int s_signum = 0;
void signal_handler(int signum)
{
    syslog(LOG_INFO, "Received signal %d", signum);
    signal(signum, signal_handler);
    s_signum = signum;
}

int main(int argc, char** argv)
{
    printf("Build Time: %s %s\n", __DATE__, __TIME__);
    if (argc > 1 && std::string(argv[1]) == "-v") {
        printf("Version: %s\n", PROJECT_VERSION);
    }

    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    http_server server(ROOT_DIR);
    if (!server.start(HTTP_PORT)) {
        printf("Failed: server.start()\n");
        return 1;
    }

    while (s_signum == 0) {
        sleep(1);
    }

    printf("%s,%d: exited\n", __func__, __LINE__);

    return 0;
}
