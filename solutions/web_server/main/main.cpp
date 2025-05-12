#include <iostream>
#include <signal.h>
#include <string>
#include <syslog.h>
#include <unistd.h>

#include "version.h"

int main(int argc, char** argv) {
    printf("Build Time: %s %s\n", __DATE__, __TIME__);
    if (argc > 1 && std::string(argv[1]) == "-v") {
        printf("Version: %s\n", PROJECT_VERSION);
    }

    for (int i = 0; i < 100; i++) {
        std::cout << "hello world" << std::endl;
        sleep(1);
    }

    return 0;
}
