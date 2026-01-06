#ifndef __LOGGER_H__
#define __LOGGER_H__

#include <chrono>
#include <cstdio>
#include <ctime>
#include <mutex>
#include <stdarg.h>
#include <string>
#include <syslog.h>

/*
#include <syslog.h>

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

class Logger {
public:
    static constexpr const char* tag[] = {
        "[EMERG]",
        "[ALERT]",
        "[CRIT]",
        "[ERROR]",
        "[WARNING]",
        "[NOTICE]",
        "[INFO]",
        "[DEBUG]",
        "[VERB]",
    };
    static const uint8_t max_level = sizeof(tag) / sizeof(tag[0]);

    Logger(const std::string& id, uint8_t level = LOG_WARNING,
        int opt = LOG_PID, int fac = LOG_USER)
    {
        _log_level = (level > max_level) ? LOG_WARNING : level;
        _log_mask = LOG_UPTO(_log_level);
        openlog(id.c_str(), opt, fac);
        setlogmask(_log_mask);
    }

    ~Logger()
    {
        closelog();
    }

    static void set_level(uint8_t level)
    {
        level = (level > max_level) ? LOG_WARNING : level;
        _log_level = level;
        _log_mask = LOG_UPTO(_log_level);
        setlogmask(_log_mask);
    }

    static void log(uint8_t level, const char* format, ...)
    {
        if (level > _log_level)
            return;

        std::lock_guard<std::mutex> lock(_log_mutex);
        va_list args;

        va_start(args, format);
        vsyslog(level, format, args);
        va_end(args);

        va_start(args, format);
        vfprintf(stdout, format, args);
        va_end(args);
    }

private:
    static inline std::mutex _log_mutex;
    static inline uint8_t _log_level = LOG_DEBUG;
    static inline int _log_mask = LOG_UPTO(_log_level);
};

#define _LOG(_level, fmt, ...) \
    Logger::log(_level, "%s %s,%d: " fmt "\n", Logger::tag[_level], __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define LOGE(fmt, ...) _LOG(LOG_ERR, fmt, ##__VA_ARGS__)
#define LOGW(fmt, ...) _LOG(LOG_WARNING, fmt, ##__VA_ARGS__)
#define LOGI(fmt, ...) _LOG(LOG_INFO, fmt, ##__VA_ARGS__)
#define LOGD(fmt, ...) _LOG(LOG_DEBUG, fmt, ##__VA_ARGS__)
#define LOGV(fmt, ...) _LOG(LOG_USER, fmt, ##__VA_ARGS__)
#define ASSERT(expr)                              \
    do {                                          \
        if (!(expr)) {                            \
            LOGE("Failed assertion '%s'", #expr); \
            abort();                              \
        }                                         \
    } while (0)
#endif // __LOGGER_H__
