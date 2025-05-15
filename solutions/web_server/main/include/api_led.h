#ifndef API_LED_H
#define API_LED_H

#include "api_base.h"

class api_led : public api_base {
public:
    api_led()
        : api_base("led")
    {
        printf("%s,%d\n", __func__, __LINE__);
        REG_API_FULL("#/#", led, true);
    }

    ~api_led()
    {
        printf("%s,%d\n", __func__, __LINE__);
    }

private:
    static api_status_t led(const json& request, json& response)
    {
        printf("%s,%d\n", __func__, __LINE__);
        return API_STATUS_OK;
    }
};

#endif // API_LED_H
