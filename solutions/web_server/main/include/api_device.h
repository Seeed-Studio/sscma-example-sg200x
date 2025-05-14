#include "api_base.h"

class api_device : public api_base {
public:
    api_device()
        : api_base("deviceMgr")
    {
        printf("%s,%d\n", __func__, __LINE__);
        list.emplace_back(REST_API_FULL("", device_api0, true));
        list.emplace_back(REST_API(device_api1));
        list.emplace_back(REST_API_FULL("device_api2/sub", device_api2, true));
    }

    ~api_device()
    {
        printf("%s,%d\n", __func__, __LINE__);
    }

    static void device_api0(mg_connection* c, mg_http_message* hm)
    {
        printf("%s,%d\n", __func__, __LINE__);
        mg_http_reply(c, 200, "Content-Type: text/plain\r\n", "%s\n", __func__);
    }

    static void device_api1(mg_connection* c, mg_http_message* hm)
    {
        printf("%s,%d\n", __func__, __LINE__);
        mg_http_reply(c, 200, "Content-Type: text/plain\r\n", "%s\n", __func__);
    }

    static void device_api2(mg_connection* c, mg_http_message* hm)
    {
        printf("%s,%d\n", __func__, __LINE__);
        mg_http_reply(c, 200, "Content-Type: text/plain\r\n", "%s\n", __func__);
    }
};