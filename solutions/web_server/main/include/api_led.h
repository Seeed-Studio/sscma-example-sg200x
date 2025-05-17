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
    static api_status_t led(const json& request, json& response)
    {
        MA_LOGV("req: %s", request.dump().c_str());

        response["code"] = 0;
        response["msg"] = "";
        response["data"] = json("");

        do {
            string uri = request["uri"].get<string>();
            size_t pos = uri.find("/");
            if (pos == string::npos) {
                return API_STATUS_NEXT;
            }
            string led = uri.substr(0, pos);
            string val = uri.substr(pos + 1);

            int value = 0;
            if (val == "on") {
                value = 100;
            }

            ofstream ofs("/sys/class/leds/" + led + "/brightness");
            if (!ofs.is_open()) {
                response["code"] = -1;
                response["msg"] = "Open led(" + led + ") failed.";
                MA_LOGE("%s", response["msg"].get<string>().c_str());
                break;
            }
            ofs << value;
            ofs.close();
        } while (0);

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
