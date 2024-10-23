
#include <iostream>
#include <syslog.h>
#include <unistd.h>

#include <sscma.h>

#include <video.h>

#include "version.h"

#include "signal.h"

#include "node/server.h"

const std::string TAG = "sscma";

using namespace ma;
using namespace ma::node;


void show_version() {
    std::cout << PROJECT_VERSION << std::endl;
}

void show_help() {
    std::cout << "Usage: sscma-node [options]\n"
              << "Options:\n"
              << "  -v, --version        Show version\n"
              << "  -h, --help           Show this help message\n"
              << "  -c, --config <file>  Configuration file, default is " << MA_NODE_CONFIG_FILE << "\n"
              << "  --start              Start the service" << std::endl;
}

int main(int argc, char** argv) {

    openlog("sscma", LOG_CONS | LOG_PERROR, LOG_DAEMON);

    MA_LOGD("main", "version: %s build: %s", PROJECT_VERSION, __DATE__ " " __TIME__);

    Signal::install({SIGINT, SIGSEGV, SIGABRT, SIGTRAP, SIGTERM, SIGHUP, SIGQUIT, SIGPIPE}, [](int sig) {
        MA_LOGE(TAG, "received signal %d", sig);
        if (sig == SIGSEGV) {
            deinitVideo();
        } else {
            NodeFactory::clear();
        }
        closelog();
        exit(1);
    });


    std::string config_file = MA_NODE_CONFIG_FILE;
    bool start_service      = false;

    if (argc < 2) {
        show_help();
        return 1;
    }

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


    if (start_service) {

        StorageFile* config = new StorageFile();
        config->init(config_file.c_str());

        MA_LOGI(TAG, "starting the service...");
        MA_LOGI(TAG, "version: %s build: %s", PROJECT_VERSION, __DATE__ " " __TIME__);
        MA_LOGI(TAG, "config: %s", config_file.c_str());

        std::string client;
        std::string host;
        int port = 1883;
        std::string user;
        std::string password;

        MA_STORAGE_GET_STR(config, MA_STORAGE_KEY_MQTT_HOST, host, "localhost");
        MA_STORAGE_GET_STR(config, MA_STORAGE_KEY_MQTT_CLIENTID, client, "recamera");
        MA_STORAGE_GET_STR(config, MA_STORAGE_KEY_MQTT_USER, user, "");
        MA_STORAGE_GET_STR(config, MA_STORAGE_KEY_MQTT_PWD, password, "");
        MA_STORAGE_GET_POD(config, MA_STORAGE_KEY_MQTT_PORT, port, 1883);


        NodeServer server(client);

        server.start(host, port, user, password);

        uint32_t count = 0;

        while (1) {
            Thread::sleep(Tick::fromSeconds(1));
        }
    }

    closelog();
    return 0;
}
