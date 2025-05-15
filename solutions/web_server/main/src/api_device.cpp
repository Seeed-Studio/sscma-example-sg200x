#include "api_device.h"

api_status_t api_device::queryDeviceInfo(const json& request, json& response)
{
    json data;

    data["appName"] = "supervisor";
    data["deviceName"] = "reCamera"; // readFile(PATH_DEVICE_NAME);
    data["sn"] = "123456789"; // sn;
    data["ip"] = "192.168.111.181"; // getDeviceIp(req->client_addr.ip);

    data["dns"] = "-";

    data["channel"] = 0; // std::stoi(ch);
    data["serverUrl"] = ""; // url;
    data["officialUrl"] = "https://github.com/Seeed-Studio/reCamera-OS/releases/latest"; // DEFAULT_UPGRADE_URL;
    data["cpu"] = "sg2002";
    data["ram"] = 256;
    data["npu"] = 1;
    data["osName"] = "reCamera"; // os;
    data["osVersion"] = "0.1.4"; // version;
    data["osUpdateTime"] = "2024.01.01";
    data["terminalPort"] = 9090; // TTYD_PORT;
    data["needRestart"] = 0; // g_restart;

    response["code"] = 0;
    response["msg"] = "";
    response["data"] = data;

    return API_STATUS_OK;
}
