#include <iomanip>
#include <random>
#include <sstream>

#include "api_wifi.h"

api_status_t api_wifi::autoConnectWiFi(const json& request, json& response)
{
    response["code"] = 0;
    response["msg"] = "";
    response["data"] = json(
        "");

    return API_STATUS_OK;
}

api_status_t api_wifi::connectWiFi(const json& request, json& response)
{
    response["code"] = 0;
    response["msg"] = "";
    response["data"] = json(
        "");

    return API_STATUS_OK;
}

api_status_t api_wifi::disconnectWiFi(const json& request, json& response)
{
    response["code"] = 0;
    response["msg"] = "";
    response["data"] = json(
        "");

    return API_STATUS_OK;
}

api_status_t api_wifi::forgetWiFi(const json& request, json& response)
{
    response["code"] = 0;
    response["msg"] = "";
    response["data"] = json(
        "");

    return API_STATUS_OK;
}

api_status_t api_wifi::getWiFiScanResults(const json& request, json& response)
{
    response["code"] = 0;
    response["msg"] = "";
    response["data"] = json(
        "");

    return API_STATUS_OK;
}

api_status_t api_wifi::getWifiStatus(const json& request, json& response)
{
    response["code"] = 0;
    response["msg"] = "";
    response["data"] = json(
        "");

    return API_STATUS_OK;
}

api_status_t api_wifi::queryWiFiInfo(const json& request, json& response)
{
    response["code"] = 0;
    response["msg"] = "";
    response["data"] = json(
        "");

    return API_STATUS_OK;
}

api_status_t api_wifi::scanWiFi(const json& request, json& response)
{
    response["code"] = 0;
    response["msg"] = "";
    response["data"] = json(
        "");

    return API_STATUS_OK;
}

api_status_t api_wifi::switchWiFi(const json& request, json& response)
{
    response["code"] = 0;
    response["msg"] = "";
    response["data"] = json(
        "");

    return API_STATUS_OK;
}
