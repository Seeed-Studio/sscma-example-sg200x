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
        MA_LOGV("");

        res["code"] = 0;
        res["msg"] = "";
        res["data"] = json("");

        do {
            string uri = get_uri(req);
            size_t pos_e = uri.rfind('/');
            if (pos_e == string::npos) {
                res["code"] = -1;
                res["msg"] = "Invalid path format";
                break;
            }
            size_t pos_s = uri.rfind('/', pos_e - 1);
            if (pos_s == string::npos) {
                pos_s = 0;
            }

            string led = uri.substr(pos_s + 1, pos_e - pos_s - 1);
            string val = uri.substr(pos_e + 1);

            MA_LOGV("led=%s, val=%s", led.c_str(), val.c_str());

            int value = 0;
            if (val == "on") {
                value = 100;
            }

            ofstream ofs("/sys/class/leds/" + led + "/brightness");
            if (!ofs.is_open()) {
                res["code"] = -1;
                res["msg"] = "Open led(" + led + ") failed.";
                MA_LOGE("%s", res["msg"].get<string>().c_str());
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
