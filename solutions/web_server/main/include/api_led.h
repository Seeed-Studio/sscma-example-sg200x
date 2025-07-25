#ifndef API_LED_H
#define API_LED_H

#include "api_base.h"

/*
    /api/led/{led_name}/on
    /api/led/{led_name}/off
    todo: /api/led/{led_name}/brightness?brightness=0
*/

class api_led : public api_base {
private:
    static api_status_t led(request_t req, response_t res)
    {
        auto&& uri = get_uri(req);
        size_t pos_e = uri.rfind('/');
        if (pos_e == std::string::npos) {
            response(res, -1, "Invalid path format");
            return API_STATUS_OK;
        }
        size_t pos_s = uri.rfind('/', pos_e - 1);
        if (pos_s == std::string::npos) {
            pos_s = 0;
        }

        const auto& led = uri.substr(pos_s + 1, pos_e - pos_s - 1);
        const auto& val = uri.substr(pos_e + 1);

        std::string led_path = "/sys/class/leds/" + led + "/brightness";
        std::string led_val = (val == "on") ? "255" : "0";
        if (std::ofstream(led_path) << led_val) {
            response(res);
        } else {
            response(res, -1, "Set led failed.");
        }
        return API_STATUS_OK;
    }

public:
    api_led()
        : api_base("led")
    {
        LOGV("");
        REG_API_FULL("", led, false); // fixed: no auth
    }

    ~api_led()
    {
        LOGV("");
    }
};

#endif // API_LED_H
