#include "api_device.h"

api_status_t api_device::cancelUpdate(const json& request, json& response)
{
    MA_LOGV("");

    response["code"] = 0;
    response["msg"] = "";
    response["data"] = json({});

    return API_STATUS_OK;
}

api_status_t api_device::getAppInfo(const json& request, json& response)
{
    MA_LOGV("");

    response["code"] = 0;
    response["msg"] = "";
    response["data"] = json({});

    return API_STATUS_OK;
}

api_status_t api_device::getDeviceInfo(const json& request, json& response)
{
    MA_LOGV("");

    response["code"] = 0;
    response["msg"] = "";
    response["data"] = json({});

    return API_STATUS_OK;
}

api_status_t api_device::getDeviceList(const json& request, json& response)
{
    MA_LOGV("");

    response["code"] = 0;
    response["msg"] = "";
    response["data"] = json({});

    return API_STATUS_OK;
}

api_status_t api_device::getModelFile(const json& request, json& response)
{
    MA_LOGV("");

    response["code"] = 0;
    response["msg"] = "";
    response["data"] = json({});

    return API_STATUS_OK;
}

api_status_t api_device::getModelInfo(const json& request, json& response)
{
    MA_LOGV("");

    response["code"] = 0;
    response["msg"] = "";
    response["data"] = json({});

    return API_STATUS_OK;
}

api_status_t api_device::getModelList(const json& request, json& response)
{
    MA_LOGV("");

    response["code"] = 0;
    response["msg"] = "";
    response["data"] = json({});

    return API_STATUS_OK;
}

api_status_t api_device::getPlatformInfo(const json& request, json& response)
{
    MA_LOGV("");

    response["code"] = 0;
    response["msg"] = "";
    response["data"] = json({});

    return API_STATUS_OK;
}

api_status_t api_device::getSystemStatus(const json& request, json& response)
{
    MA_LOGV("");

    response["code"] = 0;
    response["msg"] = "";
    response["data"] = json({});

    return API_STATUS_OK;
}

api_status_t api_device::getSystemUpdateVesionInfo(const json& request, json& response)
{
    MA_LOGV("");

    response["code"] = 0;
    response["msg"] = "";
    response["data"] = json({
        { "osName", "reCameraOS" },
        { "osVersion", "0.1.4" },
        { "downloadUrl", "abc" },
        { "isUpgrading", "0" },
    });

    return API_STATUS_OK;
}

api_status_t api_device::getUpdateProgress(const json& request, json& response)
{
    MA_LOGV("");

    response["code"] = 0;
    response["msg"] = "";
    response["data"] = json({});

    return API_STATUS_OK;
}

api_status_t api_device::queryDeviceInfo(const json& request, json& response)
{
    MA_LOGV("");

    json data({
        { "appName", "supervisor" },
        { "deviceName", "reCamera" },
        { "sn", "123456789" },
        { "ip", "192.168.111.136" },
        { "dns", "" },
        { "channel", 0 },
        { "serverUrl", "" },
        { "officialUrl", "https://github.com/Seeed-Studio/reCamera-OS/releases/latest" },
        { "cpu", "sg2002" },
        { "ram", 256 },
        { "npu", 1 },
        { "osName", "reCamera" },
        { "osVersion", "0.1.4" },
        { "osUpdateTime", "2024.01.01" },
        { "terminalPort", 9090 },
        { "needRestart", 0 },
    });

    response["code"] = 0;
    response["msg"] = "";
    response["data"] = data;

    return API_STATUS_OK;
}

api_status_t api_device::queryServiceStatus(const json& request, json& response)
{
    MA_LOGV("");

    response["code"] = 0;
    response["msg"] = "";
    response["data"] = json({
        { "sscmaNode", 0 },
        { "nodeRed", 0 },
        { "system", 0 },
        { "uptime", 100 },
        { "timestamp", 100 },
    });

    return API_STATUS_OK;
}

api_status_t api_device::savePlatformInfo(const json& request, json& response)
{
    MA_LOGV("");

    response["code"] = 0;
    response["msg"] = "";
    response["data"] = json({});

    return API_STATUS_OK;
}

api_status_t api_device::setPower(const json& request, json& response)
{
    MA_LOGV("");

    response["code"] = 0;
    response["msg"] = "";
    response["data"] = json({});

    return API_STATUS_OK;
}

api_status_t api_device::updateChannel(const json& request, json& response)
{
    MA_LOGV("%s", request.dump().c_str());

    response["code"] = 0;
    response["msg"] = "";
    response["data"] = json("");

    return API_STATUS_OK;
}

api_status_t api_device::updateDeviceName(const json& request, json& response)
{
    MA_LOGV("%s", request.dump().c_str());

    response["code"] = 0;
    response["msg"] = "";
    response["data"] = json("");

    return API_STATUS_OK;
}

api_status_t api_device::updateSystem(const json& request, json& response)
{
    MA_LOGV("%s", request.dump().c_str());

    response["code"] = 0;
    response["msg"] = "";
    response["data"] = json({});

    return API_STATUS_OK;
}

api_status_t api_device::uploadApp(const json& request, json& response)
{
    MA_LOGV("");

    response["code"] = 0;
    response["msg"] = "";
    response["data"] = json({});

    return API_STATUS_OK;
}

api_status_t api_device::uploadModel(const json& request, json& response)
{
    MA_LOGV("");

    response["code"] = 0;
    response["msg"] = "";
    response["data"] = json({});

    return API_STATUS_OK;
}
