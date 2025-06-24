#include <iostream>
#include <signal.h>
#include <stdexcept>
#include <string>
#include <syslog.h>
#include <unistd.h>

#include "http_server.h"
#include "logger.hpp"
#include "version.h"

// #define ROOT_DIR "/usr/share/supervisor/www/"
#define ROOT_DIR "./dist/"
#define HTTP_PORT "8000"

int main(int argc, char** argv)
{
    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGTERM);
    sigaddset(&sigset, SIGINT); // CTRL+C
    sigaddset(&sigset, SIGTSTP); // CTRL+Z
    pthread_sigmask(SIG_BLOCK, &sigset, nullptr);

    MA_LOG_INIT("web_server", LOG_PID | LOG_CONS, LOG_USER);
    try {
        if (argc > 1 && std::string(argv[1]) == "-v") {
            std::cout << "Build Time: " << __DATE__ << " " << __TIME__ << std::endl;
            std::cout << PROJECT_NAME << " V" << PROJECT_VERSION << std::endl;
        }

        http_server server(ROOT_DIR);
        if (!server.start(HTTP_PORT)) {
            MA_LOGE("Failed: server.start()\n");
        } else {
            int sig;
            sigwait(&sigset, &sig); // 阻塞直到收到 SIGINT/SIGTERM
            MA_LOGW("Exited with sig: ", sig);
        }
    } catch (std::exception& e) {
        MA_LOGE("Exception: ", e.what());
    } catch (...) {
        MA_LOGE("Unknown exception");
    }
    MA_LOG_DEINIT();

    return 0;
}
