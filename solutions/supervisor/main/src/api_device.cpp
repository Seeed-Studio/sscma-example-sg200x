#include <array>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <syslog.h>

#include "api_device.h"

#define BASE_API(_type, _auth, _api, _func, _class) apis.emplace_back( \
    _type, _auth, "/api/" #_api "/" #_func,                            \
    std::bind(&_class::_func, this, std::placeholders::_1, std::placeholders::_2));

#define API_GET(_func) BASE_API(API_TYPE_GET, true, deviceMgr, _func, api_device)
#define API_GET_NOAUTH(_func) BASE_API(API_TYPE_GET, false, deviceMgr, _func, api_device)
#define API_POST(_func) BASE_API(API_TYPE_POST, true, deviceMgr, _func, api_device)
#define API_POST_NOAUTH(_func) BASE_API(API_TYPE_POST, false, deviceMgr, _func, api_device)

void api_device::register_apis()
{
    API_GET(getSystemStatus);
    API_GET(queryDeviceInfo);

    for (const auto& api : apis) {
        std::cout << api << std::endl;
    }
}

int api_device::getSystemStatus(HttpRequest* req, HttpResponse* resp)
{
    std::string result = exec_shell_cmd(__func__);
    std::cout << "result:" << result << std::endl;

    hv::Json json;
    if (result.empty()) {
        json["code"] = 0;
        json["msg"] = "";
    } else {
        json["code"] = -1;
        json["msg"] = "The system was damaged and has been restored.";
    }
    json["data"] = hv::Json({});

    std::cout << "json:" << json.dump() << std::endl;
    return resp->Json(json);
}

int api_device::queryDeviceInfo(HttpRequest* req, HttpResponse* resp)
{
    std::cout << "++++++++++++++++++++>queryDeviceInfo" << std::endl;
    return 200;
}

#if 0
int api_device::queryServiceStatus(HttpRequest* req, HttpResponse* resp)
{
    hv::Json json;

    json["code"] = 0;
    json["msg"] = "";
    json["data"]["sscmaNode"] = sv_->daemon->get_sscma_status();
    json["data"]["nodeRed"] = sv_->daemon->get_nodered_status();
    json["data"]["system"] = 0;

    return resp->Json(json);
}
#endif