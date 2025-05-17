#ifndef __LOGGER_H__
#define __LOGGER_H__

#include <atomic>
#include <chrono>
#include <ctime>
#include <mutex>
#include <sstream>
#include <syslog.h>
#include <thread>

/*
#define LOG_EMERG   0
#define LOG_ALERT   1
#define LOG_CRIT    2
#define LOG_ERR     3
#define LOG_WARNING 4
#define LOG_NOTICE  5
#define LOG_INFO    6
#define LOG_DEBUG   7

#define LOG_USER     (1<<3)
*/

#ifndef MA_LOG_MASK
#define MA_LOG_MASK (LOG_UPTO(LOG_USER))
#endif

#define MA_LOG_INIT(_id, _opt, _fac) \
    do {                             \
        openlog(_id, _opt, _fac);    \
        setlogmask(MA_LOG_MASK);     \
    } while (0)

#define MA_LOG_DEINIT() \
    do {                \
        closelog();     \
    } while (0)

#define MA_LOG(_prefix, _lvl, _fmt, ...)                                                \
    do {                                                                                \
        static std::mutex log_mutex;                                                    \
        std::lock_guard<std::mutex> lock(log_mutex);                                    \
        char buf[512];                                                                  \
        snprintf(buf, sizeof(buf), "[%s:%d] " _fmt, __func__, __LINE__, ##__VA_ARGS__); \
        if (MA_LOG_MASK & (1 << _lvl)) {                                                \
            std::cout << _prefix << buf << std::endl;                                   \
        }                                                                               \
        syslog(_lvl, "%s", buf);                                                        \
    } while (0)

#define MA_LOGE(_fmt, ...) MA_LOG("[E]", LOG_ERR, _fmt, ##__VA_ARGS__)
#define MA_LOGW(_fmt, ...) MA_LOG("[W]", LOG_WARNING, _fmt, ##__VA_ARGS__)
#define MA_LOGI(_fmt, ...) MA_LOG("[I]", LOG_INFO, _fmt, ##__VA_ARGS__)
#define MA_LOGD(_fmt, ...) MA_LOG("[D]", LOG_DEBUG, _fmt, ##__VA_ARGS__)
#define MA_LOGV(_fmt, ...) MA_LOG("[V]", LOG_USER, _fmt, ##__VA_ARGS__)
#define MA_ASSERT(expr)                              \
    do {                                             \
        if (!(expr)) {                               \
            MA_LOGE("Failed assertion '%s'", #expr); \
            abort();                                 \
        }                                            \
    } while (0)

#endif // __LOGGER_H__
