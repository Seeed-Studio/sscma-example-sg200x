#ifndef __LOGGER_H__
#define __LOGGER_H__

#include <atomic>
#include <chrono>
#include <ctime>
#include <mutex>
#include <sstream>
#include <syslog.h>
#include <thread>

#ifndef MA_LOG_MASK
#define MA_LOG_MASK (LOG_UPTO(LOG_DEBUG))
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

#define MA_LOG(_text, _lvl, _tag, _fmt, ...)                               \
    do {                                                                   \
        static std::mutex log_mutex;                                       \
        std::lock_guard<std::mutex> lock(log_mutex);                       \
        std::ostringstream oss;                                            \
        oss << "[" << __func__ << ":" << __LINE__ << "][" << _tag << "] "; \
        oss << _fmt;                                                       \
        if (MA_LOG_MASK & (1 << _lvl)) {                                   \
            std::cout << oss.str() << std::endl;                           \
        }                                                                  \
        syslog(_lvl, "%s", oss.str().c_str());                             \
    } while (0)

#define MA_LOGE(_tag, _fmt, ...) MA_LOG("[E]", LOG_ERR, _tag, _fmt, ##__VA_ARGS__)
#define MA_LOGW(_tag, _fmt, ...) MA_LOG("[W]", LOG_WARNING, _tag, _fmt, ##__VA_ARGS__)
#define MA_LOGI(_tag, _fmt, ...) MA_LOG("[I]", LOG_INFO, _tag, _fmt, ##__VA_ARGS__)
#define MA_LOGD(_tag, _fmt, ...) MA_LOG("[D]", LOG_DEBUG, _tag, _fmt, ##__VA_ARGS__)
#define MA_LOGV(_tag, _fmt, ...) MA_LOG("[V]", LOG_DEBUG, _tag, _fmt, ##__VA_ARGS__)
#define MA_ASSERT(_tag, expr)                              \
    do {                                                   \
        if (!(expr)) {                                     \
            MA_LOGE(_tag, "Failed assertion '%s'", #expr); \
            abort();                                       \
        }                                                  \
    } while (0)

#endif // __LOGGER_H__
