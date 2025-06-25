#include <signal.h>
#include <string>

#include "http_server.h"

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

    Logger logger("web_server", LOG_USER);

    try {
        if (argc > 1 && std::string(argv[1]) == "-v") {
            LOGI("Build Time: %s %s", __DATE__, __TIME__);
            LOGI("%s V%s", PROJECT_NAME, PROJECT_VERSION);
        }

        http_server server(ROOT_DIR);
        if (!server.start(HTTP_PORT)) {
            LOGE("Failed: server.start()");
        } else {
            int sig;
            sigwait(&sigset, &sig); // 阻塞直到收到 SIGINT/SIGTERM
            LOGI("Exited with sig: %d", sig);
        }
    } catch (std::exception& e) {
        LOGE("Exception: %s", e.what());
    } catch (...) {
        LOGE("Unknown exception");
    }
    LOGV("");

    return 0;
}
