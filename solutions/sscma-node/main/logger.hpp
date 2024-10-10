#pragma once

#include <syslog.h>
#include <unistd.h>

#define MA_LOGE(TAG, ...)             \
    do {                              \
        syslog(LOG_ERR, __VA_ARGS__); \
    } while (0)


#define MA_LOGW(TAG, ...)                 \
    do {                                  \
        syslog(LOG_WARNING, __VA_ARGS__); \
    } while (0)

#define MA_LOGI(TAG, ...)              \
    do {                               \
        syslog(LOG_INFO, __VA_ARGS__); \
    } while (0)


#define MA_LOGD(TAG, ...)               \
    do {                                \
        syslog(LOG_DEBUG, __VA_ARGS__); \
    } while (0)

#define MA_LOGV(TAG, ...)               \
    do {                                \
        syslog(LOG_DEBUG, __VA_ARGS__); \
    } while (0)


#define MA_ASSERT(expr)                                      \
    do {                                                     \
        if (!(expr)) {                                       \
            syslog(LOG_ERR, "Failed assertion '%s'", #expr); \
            while (1) {                                      \
                ma_abort();                                  \
            }                                                \
        }                                                    \
    } while (0)
