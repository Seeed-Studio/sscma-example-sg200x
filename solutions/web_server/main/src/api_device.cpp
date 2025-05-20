#include "api_device.h"
#include "api_file.h"

api_status_t api_device::cancelUpdate(request_t req, response_t res)
{
    MA_LOGV("");

    res["code"] = 0;
    res["msg"] = "";
    res["data"] = json({});

    return API_STATUS_OK;
}

api_status_t api_device::getAppInfo(request_t req, response_t res)
{
    MA_LOGV("");

    res["code"] = 0;
    res["msg"] = "";
    res["data"] = json({});

    return API_STATUS_OK;
}

api_status_t api_device::getCameraWebsocketUrl(request_t req, response_t res)
{
    MA_LOGV("");

    res["code"] = 0;
    res["msg"] = "";
    res["data"] = json({});

    return API_STATUS_OK;
}

api_status_t api_device::getDeviceInfo(request_t req, response_t res)
{
    MA_LOGV("");

    res["code"] = 0;
    res["msg"] = "";
    res["data"] = json({});

    return API_STATUS_OK;
}

api_status_t api_device::getDeviceList(request_t req, response_t res)
{
    MA_LOGV("");

    res["code"] = 0;
    res["msg"] = "";
    res["data"] = json({});

    return API_STATUS_OK;
}

api_status_t api_device::getModelFile(request_t req, response_t res)
{
    MA_LOGV("");

    res["code"] = 0;
    res["msg"] = "";
    res["data"] = json({});

    return API_STATUS_OK;
}

api_status_t api_device::getModelInfo(request_t req, response_t res)
{
    MA_LOGV("");

    res["code"] = 0;
    res["msg"] = "";
    res["data"] = json({});

    return API_STATUS_OK;
}

api_status_t api_device::getModelList(request_t req, response_t res)
{
    MA_LOGV("");

    res["code"] = 0;
    res["msg"] = "";
    res["data"] = json({});

    return API_STATUS_OK;
}

api_status_t api_device::getPlatformInfo(request_t req, response_t res)
{
    MA_LOGV("");

    res["code"] = 0;
    res["msg"] = "";
    res["data"] = json({});

    return API_STATUS_OK;
}

api_status_t api_device::getSystemStatus(request_t req, response_t res)
{
    MA_LOGV("");

    res["code"] = 0;
    res["msg"] = "";
    res["data"] = json({});

    return API_STATUS_OK;
}

api_status_t api_device::getSystemUpdateVesionInfo(request_t req, response_t res)
{
    MA_LOGV("");

    res["code"] = 0;
    res["msg"] = "";
    res["data"] = json({
        { "osName", "reCameraOS" },
        { "osVersion", "0.1.4" },
        { "downloadUrl", "abc" },
        { "isUpgrading", "0" },
    });

    return API_STATUS_OK;
}

api_status_t api_device::getUpdateProgress(request_t req, response_t res)
{
    MA_LOGV("");

    res["code"] = 0;
    res["msg"] = "";
    res["data"] = json({});

    return API_STATUS_OK;
}

api_status_t api_device::queryDeviceInfo(request_t req, response_t res)
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

    res["code"] = 0;
    res["msg"] = "";
    res["data"] = data;

    return API_STATUS_OK;
}

api_status_t api_device::queryServiceStatus(request_t req, response_t res)
{
    MA_LOGV("");

    res["code"] = 0;
    res["msg"] = "";
    res["data"] = json({
        { "sscmaNode", 0 },
        { "nodeRed", 0 },
        { "system", 0 },
        { "uptime", 100 },
        { "timestamp", 100 },
    });

    return API_STATUS_OK;
}

api_status_t api_device::savePlatformInfo(request_t req, response_t res)
{
    MA_LOGV("");

    res["code"] = 0;
    res["msg"] = "";
    res["data"] = json({});

    return API_STATUS_OK;
}

api_status_t api_device::setPower(request_t req, response_t res)
{
    MA_LOGV("");

    res["code"] = 0;
    res["msg"] = "";
    res["data"] = json({});

    return API_STATUS_OK;
}

api_status_t api_device::updateChannel(request_t req, response_t res)
{
    MA_LOGV("");

    res["code"] = 0;
    res["msg"] = "";
    res["data"] = json("");

    return API_STATUS_OK;
}

api_status_t api_device::updateDeviceName(request_t req, response_t res)
{
    MA_LOGV("");

    res["code"] = 0;
    res["msg"] = "";
    res["data"] = json("");

    return API_STATUS_OK;
}

api_status_t api_device::updateSystem(request_t req, response_t res)
{
    MA_LOGV("");

    res["code"] = 0;
    res["msg"] = "";
    res["data"] = json({});

    return API_STATUS_OK;
}

api_status_t api_device::uploadApp(request_t req, response_t res)
{
    MA_LOGV("");

    res["code"] = 0;
    res["msg"] = "";
    res["data"] = json({});

    return API_STATUS_OK;
}

api_status_t api_device::uploadModel(request_t req, response_t res)
{
    MA_LOGV("");

    res["code"] = 0;
    res["msg"] = "";
    res["data"] = json({});

    return API_STATUS_OK;
}
