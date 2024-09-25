
#include <iostream>
#include <syslog.h>
#include <unistd.h>

#include <sscma.h>

#include "version.h"

#include "signal.h"

const std::string TAG = "sscma";

void show_version() {
    std::cout << PROJECT_VERSION << std::endl;
}

void show_help() {
    std::cout << "Usage: sscma-node [options]\n"
              << "Options:\n"
              << "  -v, --version        Show version\n"
              << "  -h, --help           Show this help message\n"
              << "  -c, --config <file>  Configuration file, default is " << SSCMA_NODE_CONFIG_FILE
              << "\n"
              << "  --start              Start the service" << std::endl;
}

int main(int argc, char** argv) {

    openlog("sscma", LOG_CONS | LOG_PERROR, LOG_DAEMON);

    MA_LOGD("main", "version: %s", PROJECT_VERSION);

    Signal::install({SIGINT, SIGSEGV, SIGABRT, SIGTRAP, SIGTERM, SIGHUP, SIGQUIT, SIGPIPE},
                    [](int sig) {
                        MA_LOGE("main", "Caught signal %d exit", sig);
                        closelog();
                        exit(0);
                    });


    std::string config_file = SSCMA_NODE_CONFIG_FILE;
    bool start_service      = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-v" || arg == "--version") {
            show_version();
            return 0;
        } else if (arg == "-h" || arg == "--help") {
            show_help();
            return 0;
        } else if (arg == "-c" || arg == "--config") {
            if (i + 1 < argc) {
                config_file = argv[++i];
            } else {
                std::cerr << "Error: Missing argument for --config" << std::endl;
                return 1;
            }
        } else if (arg == "--start") {
            start_service = true;
        } else {
            std::cerr << "Error: Unknown option " << arg << std::endl;
            return 1;
        }
    }

    StorageFile config(config_file);

    config.set("version", PROJECT_VERSION);

    if (start_service) {
        uint32_t count = 0;
        MA_LOGI(TAG, "Starting the service...");
        MA_LOGI(TAG, "Version: %s", PROJECT_VERSION);
        MA_LOGI(TAG, "Config: %s", config_file.c_str());
        while (1) {
            Thread::sleep(Tick::fromSeconds(1));
        }
    }

    closelog();
    return 0;
}
