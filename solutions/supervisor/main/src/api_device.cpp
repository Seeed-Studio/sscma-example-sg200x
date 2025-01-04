#include <array>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

#include "api_device.h"

#undef TAG
#define TAG "api_device"

#define API_GROUP "deviceMgr"
#define CLASS_TYPE api_device

void api_device::register_apis()
{
    SYNC_API(GET, queryServiceStatus);
    SYNC_API(GET, queryDeviceInfo);
    SYNC_API(POST, updateDeviceName);
    SYNC_API(POST, updateChannel);
    SYNC_API(POST, setPower);
    SYNC_API(POST, getDeviceList);
    SYNC_API(GET, getDeviceInfo);

    SYNC_API(GET, getModelInfo);
    // SYNC_API(GET, getModelFile);
    // SYNC_API(POST, uploadModel);
    // SYNC_API(GET, getModelList);

    // API_POST(savePlatformInfo);
    // API_GET(getPlatformInfo);

    for (const auto& api : apis) {
        std::cout << api << std::endl;
    }
}

int api_device::queryServiceStatus(HttpRequest* req, HttpResponse* resp)
{
    hv::Json json;

    json["code"] = 0;
    json["msg"] = "";
    json["data"]["sscmaNode"] = (nullptr != sscma_status) ? sscma_status() : APP_STATUS_UNKOWN;
    json["data"]["nodeRed"] = (nullptr != nodered_status) ? nodered_status() : APP_STATUS_UNKOWN;
    json["data"]["system"] = system_status_;

    return resp->Json(json);
}

int api_device::queryDeviceInfo(HttpRequest* req, HttpResponse* resp)
{
    // std::string wifiStatus;
    // std::string os_version = readFile(PATH_ISSUE);
    // std::string os = "Null", version = "Null";
    // size_t pos = os_version.find(' ');
    // if (pos != std::string::npos) {
    //     os      = os_version.substr(0, pos);
    //     version = os_version.substr(pos + 1);
    // }
    // if (version.back() == '\n') {
    //     version.erase(version.size() - 1);
    // }

    // std::string ch_url = readFile(PATH_UPGRADE_URL);
    // std::string ch = "0", url = "";
    // pos = ch_url.find(',');
    // if (pos != std::string::npos) {
    //     ch  = ch_url.substr(0, pos);
    //     url = ch_url.substr(pos + 1);
    // }
    // if (url.back() == '\n') {
    //     url.erase(url.size() - 1);
    // }

    // std::string wifiStatus = "";//getWifiConnectStatus();

    hv::Json data;

    data["appName"] = "supervisor";
    data["deviceName"] = "reCamera"; // readFile(PATH_DEVICE_NAME);
    data["sn"] = "xxxxx"; // sn;
    data["ip"] = "192.168.111.181"; // getDeviceIp(req->client_addr.ip);

    // if (wifiStatus == "COMPLETED" && isLegalWifiIp()) {
    //     data["wifiIp"]  = getIpAddress("wlan0");
    //     data["mask"]    = getNetmaskAddress("wlan0");
    //     data["gateway"] = getGateWay(data["wifiIp"]);
    // } else {
    //     data["wifiIp"]  = "-";
    //     data["mask"]    = "-";
    //     data["gateway"] = "-";
    // }
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

    hv::Json json;
    json["code"] = 0;
    json["msg"] = "";
    json["data"] = data;

    // output
    // syslog(LOG_INFO, "Device name: %s\n", data["deviceName"].template get<std::string>().c_str());
    // syslog(LOG_INFO, "OS,Version: %s\n", os_version.c_str());
    // syslog(LOG_INFO, "Channel,Url: %s\n", ch_url.c_str());

    return resp->Json(
        hv::Json({
            { "code", 0 },
            { "msg", "" },
            { "data", data },
        }));
}

int api_device::updateDeviceName(HttpRequest* req, HttpResponse* resp)
{
    std::string new_name = req->GetString("deviceName");

    MA_LOGD(TAG, "new_name: %s", new_name.c_str());
    run_script(std::string(__func__), new_name);

    return resp->Json(
        hv::Json({
            { "code", 0 },
            { "msg", "" },
            { "data", hv::Json({}) },
        }));
}

int api_device::updateChannel(HttpRequest* req, HttpResponse* resp)
{
    std::string new_ch = req->GetString("channel");
    std::string new_url = req->GetString("serverUrl");

    MA_LOGD(TAG, "new_ch: %s, new_url: %s", new_ch.c_str(), new_url.c_str());
    run_script(std::string(__func__), new_ch + " " + new_url);

    return resp->Json(
        hv::Json({
            { "code", 0 },
            { "msg", "" },
            { "data", hv::Json({}) },
        }));
}

int api_device::setPower(HttpRequest* req, HttpResponse* resp)
{
    int mode = stoi(req->GetString("mode"));
    const char* action = (mode == 0) ? "poweroff" : "reboot";

    MA_LOGW(TAG, "%s: %s", __func__, action);
    run_script(std::string(__func__), action);

    return resp->Json(
        hv::Json({
            { "code", 0 },
            { "msg", "" },
            { "data", hv::Json({}) },
        }));
}

int api_device::getDeviceList(HttpRequest* req, HttpResponse* resp)
{
    std::string result = run_script(std::string(__func__)); // generate result

    run_script(std::string(__func__), result); // rm result

    return resp->Json(
        hv::Json({
            { "code", 0 },
            { "msg", "" },
            { "data", hv::Json({}) },
        }));
}

int api_device::getDeviceInfo(HttpRequest* req, HttpResponse* resp)
{
    std::string result = run_script("getAddress", req->client_addr.ip);

    hv::Json data;
    data["ip"] = result;
    data["osVersion"] = os_version_;
    data["deviceName"] = os_name_;
    data["sn"] = sn_;
    data["status"] = 1;

    return resp->Json(
        hv::Json({
            { "code", 0 },
            { "msg", "" },
            { "data", data },
        }));
}

int api_device::getModelInfo(HttpRequest* req, HttpResponse* resp)
{
    std::string json_file;
    std::string model_file;
    std::string model_md5;
    std::string result = run_script(std::string(__func__));

    std::istringstream iss(result);
    std::getline(iss, json_file, '\n');
    std::getline(iss, model_file, '\n');
    std::getline(iss, model_md5, '\n');

    int code = -1;
    std::string msg = "File does not exist";
    hv::Json data({});
    if (!(json_file.empty() || model_file.empty() || model_md5.empty())) {
        data["model_info"] = read_file(json_file);
        data["model_md5"] = model_md5;
        code = 0;
        msg = "";
    }

    return resp->Json(
        hv::Json({
            { "code", code },
            { "msg", msg },
            { "data", data },
        }));
}