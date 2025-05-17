#ifndef API_LED_H
#define API_LED_H

#include "api_base.h"

class api_led : public api_base {
private:
    static api_status_t led(const json& request, json& response)
    {
        response["code"] = 0;
        response["msg"] = "";
        response["data"] = json(
            "");
        return API_STATUS_OK;
    }

public:
    api_led()
        : api_base("led")
    {
        MA_LOGV("");

        REG_API_FULL("#/#", led, false); // fixed: no auth
    }

    ~api_led()
    {
        MA_LOGV("");
    }
};

#endif // API_LED_H
