#include <cstdarg>
#include <cstdio>
#include <iostream>
#include <signal.h>
#include <string>
#include <thread>
#include <unistd.h>

#include "supervisor.h"

#undef TAG
#define TAG "main"

std::atomic<int> log_to_std_mask { 0 };
std::string format_log(const char* format, ...)
{
    char buffer[256];

    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    return std::string(buffer);
}

static std::atomic<bool> main_loop_exit;
static void exitHandle(int signo)
{
    MA_LOGI(TAG, "received signal %d", signo);
    main_loop_exit = true;
    // exit(0);
}

int main(int argc, char** argv)
{
    MA_LOG_INIT("supervisor", LOG_CONS | LOG_PID, 0);
    MA_LOG_MASK(LOG_UPTO(LOG_DEBUG));

    signal(SIGINT, &exitHandle);
    signal(SIGTERM, &exitHandle);

    auto ptr_app = new supervisor;
    ptr_app->start();

    main_loop_exit = false;
    while (!main_loop_exit) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    MA_LOGI(TAG, "*****end*****");

    delete ptr_app;
    MA_LOG_DEINIT();

    return 0;
}
