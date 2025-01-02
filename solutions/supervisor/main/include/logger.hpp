#pragma once

extern std::atomic<int> log_to_std_mask;
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

#define MA_LOG(_text, _type, TAG, ...)                                                          \
    do {                                                                                        \
        std::string msg = format_log("[" TAG "]" __VA_ARGS__);                                  \
        if (log_to_std_mask & _type) {                                                          \
            std::string prefix = std::string(__func__) + "," + std::to_string(__LINE__) + ": "; \
            std::cout << _text << prefix << msg << std::endl;                                   \
        }                                                                                       \
        syslog(_type, "%s", msg.c_str());                                                       \
    } while (0)

#define MA_LOGE(TAG, ...) MA_LOG("[E]", LOG_ERR, TAG, __VA_ARGS__)
#define MA_LOGW(TAG, ...) MA_LOG("[W]", LOG_WARNING, TAG, __VA_ARGS__)
#define MA_LOGI(TAG, ...) MA_LOG("[I]", LOG_INFO, TAG, __VA_ARGS__)
#define MA_LOGD(TAG, ...) MA_LOG("[D]", LOG_DEBUG, TAG, __VA_ARGS__)
#define MA_LOGV(TAG, ...) MA_LOG("[V]", LOG_DEBUG, TAG, __VA_ARGS__)
#define MA_ASSERT(TAG, expr)                              \
    do {                                                  \
        if (!(expr)) {                                    \
            MA_LOGE(TAG, "Failed assertion '%s'", #expr); \
            abort();                                      \
        }                                                 \
    } while (0)
