#ifndef API_LED_H
#define API_LED_H

#include "api_base.h"

class api_led : public api_base {
public:
    api_led()
        : api_base("led")
    {
        MA_LOGV("");
        REG_API_FULL("#/#", led, true);
    }

    ~api_led()
    {
        MA_LOGV("");
    }

private:
    static api_status_t led(const json& request, json& response)
    {
        MA_LOGV("");
        return API_STATUS_OK;
    }
};

#endif // API_LED_H
