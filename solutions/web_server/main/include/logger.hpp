#ifndef __LOGGER_H__
#define __LOGGER_H__

#include <string>
#include <syslog.h>

extern int log_to_std_mask;
extern std::string format_log(const char* format, ...);

#define MA_LOG_INIT(_id, _opt, _fac) \
    do {                             \
        openlog(_id, _opt, _fac);    \
    } while (0)

#define MA_LOG_MASK(_mask)       \
    do {                         \
        log_to_std_mask = _mask; \
        setlogmask(_mask);       \
    } while (0)

#define MA_LOG_DEINIT()      \
    do {                     \
        log_to_std_mask = 0; \
        closelog();          \
    } while (0)

#define MA_LOG(_text, _type, _tag, _fmt, ...)                                                   \
    do {                                                                                        \
        std::string msg;                                                                        \
        msg = format_log("[%s] " _fmt, _tag, ##__VA_ARGS__);                                    \
        if (log_to_std_mask & (1 << _type)) {                                                   \
            std::string prefix = std::string(__func__) + "," + std::to_string(__LINE__) + ": "; \
            std::cout << _text << prefix << msg << std::endl;                                   \
        }                                                                                       \
        syslog(_type, "%s", msg.c_str());                                                       \
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
