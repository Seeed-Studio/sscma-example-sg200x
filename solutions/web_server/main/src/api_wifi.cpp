#include <iomanip>
#include <random>
#include <sstream>

#include "api_wifi.h"

api_status_t api_wifi::autoConnectWiFi(request_t req, response_t res)
{
    res["code"] = 0;
    res["msg"] = "";
    res["data"] = json(
        "");

    return API_STATUS_OK;
}

api_status_t api_wifi::connectWiFi(request_t req, response_t res)
{
    res["code"] = 0;
    res["msg"] = "";
    res["data"] = json(
        "");

    return API_STATUS_OK;
}

api_status_t api_wifi::disconnectWiFi(request_t req, response_t res)
{
    res["code"] = 0;
    res["msg"] = "";
    res["data"] = json(
        "");

    return API_STATUS_OK;
}

api_status_t api_wifi::forgetWiFi(request_t req, response_t res)
{
    res["code"] = 0;
    res["msg"] = "";
    res["data"] = json(
        "");

    return API_STATUS_OK;
}

api_status_t api_wifi::getWiFiScanResults(request_t req, response_t res)
{
    res["code"] = 0;
    res["msg"] = "";
    res["data"] = json(
        "");

    return API_STATUS_OK;
}

api_status_t api_wifi::getWifiStatus(request_t req, response_t res)
{
    res["code"] = 0;
    res["msg"] = "";
    res["data"] = json(
        "");

    return API_STATUS_OK;
}

api_status_t api_wifi::queryWiFiInfo(request_t req, response_t res)
{
    res["code"] = 0;
    res["msg"] = "";
    res["data"] = json(
        "");

    return API_STATUS_OK;
}

api_status_t api_wifi::scanWiFi(request_t req, response_t res)
{
    res["code"] = 0;
    res["msg"] = "";
    res["data"] = json(
        "");

    return API_STATUS_OK;
}

api_status_t api_wifi::switchWiFi(request_t req, response_t res)
{
    res["code"] = 0;
    res["msg"] = "";
    res["data"] = json(
        "");

    return API_STATUS_OK;
}
