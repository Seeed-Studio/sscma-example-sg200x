#include "api_device.h"
#include "api_file.h"
#include <iterator>

api_status_t api_device::getCameraWebsocketUrl(request_t req, response_t res)
{
    std::stringstream ss;
    ss << "ws://" << get_host(req) << ":" << _dev_info.value("ws", "");
    response(res, 0, STR_OK, { { "websocketUrl", ss.str() } });
    return API_STATUS_OK;
}

api_status_t api_device::getDeviceInfo(request_t req, response_t res)
{
    json data = json::object();
    data["ip"] = get_host(req);
    data["port"] = get_port(req);
    data["sn"] = _dev_info.value("sn", "");
    data["deviceName"] = _dev_info.value("deviceName", "");
    data["status"] = _dev_info.value("status", 1);
    data["osName"] = _dev_info.value("osName", "");
    data["osVersion"] = _dev_info.value("osVersion", "");
    response(res, 0, STR_OK, data);
    return API_STATUS_OK;
}

api_status_t api_device::getDeviceList(request_t req, response_t res)
{
    using namespace std;
    std::ifstream file(script(__func__));
    if (!file.is_open()) {
        response(res, 0, STR_OK, { { "deviceList", "" } });
        return API_STATUS_OK;
    }

    json dev_list;
    map<pair<string, string>, map<string, string>> devs_map;
    string line;
    while (getline(file, line)) {
        if (line.empty() || line[0] != '=')
            continue;

        auto&& it = string_split(line, ';');
        if (it.size() < 10)
            continue;

        string type = it[1];
        string service = it[4];
        string ip = it[7];
        string port = it[8];
        if ((service == "_sscma._tcp")
            && (string(it[9]).find("sn=") != string::npos)) {
            dev_list.push_back({ { "type", type },
                { "device", it[3] },
                { "domain", it[6] },
                { "ip", ip },
                { "info", it[9] } });
        }
        devs_map[std::make_pair(type, ip)][service] = port;
    }

    for (auto& dev : dev_list) {
        auto it = devs_map.find(std::make_pair(dev["type"], dev["ip"]));
        if (it != devs_map.end()) {
            for (auto& [service, port] : it->second) {
                dev["services"][service] = port;
            }
        }
    }
    file.close();
    response(res, 0, STR_OK, { { "deviceList", dev_list } });
    return API_STATUS_OK;
}

api_status_t api_device::updateDeviceName(request_t req, response_t res)
{
    std::string result = script(__func__, parse_body(req).value("deviceName", ""));
    response(res, (result == STR_OK) ? 0 : -1, result);
    return API_STATUS_OK;
}

api_status_t api_device::queryDeviceInfo(request_t req, response_t res)
{
    auto&& data = parse_result(script(__func__));
    LOGV("queryDeviceInfo: %s", data.dump().c_str());
    data["appName"] = PROJECT_NAME;
    data["ip"] = get_host(req);
    data["port"] = get_port(req);
    data["officialUrl"] = _dev_info.value("url", "");
    data["sn"] = _dev_info.value("sn", "");
    data["osName"] = _dev_info.value("os", "");
    data["osVersion"] = _dev_info.value("ver", "");
    data["cpu"] = _dev_info.value("cpu", "");
    data["ram"] = _dev_info.value("ram", "");
    data["npu"] = _dev_info.value("npu", "");
    data["terminalPort"] = _dev_info.value("ttyd", "");
    response(res, 0, STR_OK, data);
    return API_STATUS_OK;
}

api_status_t api_device::getSystemStatus(request_t req, response_t res)
{
    std::string rollback = _dev_info.value("rollback", "");
    if (rollback == "1")
        response(res, 0, STR_OK, "System has been recovered from damage.");
    else
        response(res, 0, STR_OK, "System is running normally.");
    return API_STATUS_OK;
}

api_status_t api_device::queryServiceStatus(request_t req, response_t res)
{
    json data = json::object();
    data["sscmaNode"] = 0;
    data["nodeRed"] = 0;
    data["system"] = 0;
    data["uptime"] = uptime();
    data["timestamp"] = timestamp();
    response(res, 0, STR_OK, data);
    return API_STATUS_OK;
}

api_status_t api_device::setPower(request_t req, response_t res)
{
    auto&& body = parse_body(req);
    int mode = body.value("mode", -1);
    int ret = -1;
    std::string status = "Invalid mode";
    if (mode == 0) {
        status = "poweroff";
        ret = 0;
    } else if (mode == 1) {
        status = "reboot";
        ret = 0;
    }
    response(res, ret, status);
    if (ret == 0)
        system(status.c_str());
    return API_STATUS_OK;
}

// APP
api_status_t api_device::getAppInfo(request_t req, response_t res)
{
    // Todo:
    response(res);
    return API_STATUS_OK;
}

api_status_t api_device::uploadApp(request_t req, response_t res)
{
    // Todo:
    response(res);
    return API_STATUS_OK;
}

// Model
api_status_t api_device::getModelFile(request_t req, response_t res)
{
    auto&& data = parse_result(script("get_model_info"));
    response(res, 0, STR_OK, data);
    return API_STATUS_REPLY_FILE;
}

api_status_t api_device::getModelInfo(request_t req, response_t res)
{
    auto&& data = parse_result(script("get_model_info"));
    std::ifstream file(data.value("info", ""));
    if (!file.is_open()) {
        response(res, 0, STR_OK, { { "model_info", "" } });
        return API_STATUS_OK;
    }
    try {
        file >> data["model_info"];
        response(res, 0, STR_OK, data);
    } catch (const json::exception& e) {
        response(res, -1, "Invalid model info.");
    }
    return API_STATUS_OK;
}

api_status_t api_device::getModelList(request_t req, response_t res)
{
    auto&& list = parse_result(script(__func__));
    if (list.empty()) {
        response(res, -1);
        return API_STATUS_OK;
    }
    const auto& suffix = list.value("suffix", "");

    int count = 0;
    json data;
    for (auto&& item : list["list"]) { // Compatible with previous
        if (auto fname = item.get<std::string>(); !fname.empty()) {
            auto&& info = parse_result(std::ifstream(fname + ".json"));
            info["id"] = info.value("model_id", "");
            info["name"] = info.value("model_name", "");
            info["md5"] = info.value("checksum", "");
            info["file"] = fname + suffix;
            data["list"].push_back(info);
            count++;
        }
    }
    data["count"] = count;
    response(res, 0, STR_OK, data);
    return API_STATUS_OK;
}

api_status_t api_device::uploadModel(request_t req, response_t res)
{
    std::string dir = _dev_info.value("model_dir", "");
    if (dir.empty()) {
        response(res, -1, "Directory is not accessible.");
        return API_STATUS_OK;
    }

    int count = 0;
    json data;
    auto&& parts = get_multiparts(req, "model_file");
    for (auto& part : parts) {
        if (part.filename.empty() || part.len == 0)
            continue;
        std::ofstream file(dir + "/" + part.filename, std::ios::binary);
        if (!file.is_open())
            continue;
        file.write(part.data, part.len);
        file.close();
        data["list"].push_back(part.filename);
        count++;
    }
    data["count"] = count;

    LOGV("Upload model file: %s", data.dump(4).c_str());
    response(res, 0, STR_OK, data);
    return API_STATUS_OK;
}

// Platform
api_status_t api_device::getPlatformInfo(request_t req, response_t res)
{
    auto&& data = parse_result(std::ifstream(_dev_info.value("platform_info", "")));
    if (data.empty() || data.value("platform_info", "").empty()) {
        response(res, -1, "Get platform info failed.");
        return API_STATUS_OK;
    }
    // LOGV("Platform info: %s", data.dump().c_str());
    response(res, 0, STR_OK, data);
    return API_STATUS_OK;
}

api_status_t api_device::savePlatformInfo(request_t req, response_t res)
{
    auto&& body = parse_body(req);
    if (body.empty() || body.value("platform_info", "").empty()) {
        response(res, -1, "Platform info is empty.");
        return API_STATUS_OK;
    }
    // LOGV("Save platform info: %s", body.dump().c_str());
    std::ofstream file(_dev_info.value("platform_info", ""), std::ios::out | std::ios::trunc);
    if (file.is_open() && file << body.dump()) {
        response(res);
    } else {
        response(res, -1, "Save platform info failed.");
    }
    return API_STATUS_OK;
}

// Upgrade
api_status_t api_device::updateChannel(request_t req, response_t res)
{
    auto&& body = parse_body(req);
    auto ch = 0;
    if (body.contains("channel")) {
        if (body["channel"].is_number_integer()) {
            ch = body["channel"].get<int>();
        } else if (body["channel"].is_string()) {
            try {
                ch = std::stoi(body["channel"].get<std::string>());
            } catch (...) {
                ch = 0;
            }
        }
    }
    auto url = body.value("serverUrl", "");
    if (ch != 0) {
        if (!url.empty()
            && url.compare(0, 7, "http://") != 0
            && url.compare(0, 8, "https://") != 0) {
            response(res, -1, "Server url is invalid.");
            return API_STATUS_OK;
        }
    }
    std::string result = script(__func__, ch, url);
    response(res, (result == STR_OK) ? 0 : -1, result);
    return API_STATUS_OK;
}

api_status_t api_device::cancelUpdate(request_t req, response_t res)
{
    script(__func__);
    response(res);
    return API_STATUS_OK;
}

api_status_t api_device::getSystemUpdateVersion(request_t req, response_t res)
{
    auto&& result = script(__func__);
    auto&& data = parse_result(result);
    LOGV("%s", data.dump().c_str());
    if (data.empty()) {
        response(res, -1, result);
        return API_STATUS_OK;
    }
    response(res, 0, STR_OK, data);
    return API_STATUS_OK;
}

api_status_t api_device::getUpdateProgress(request_t req, response_t res)
{
    auto&& result = script(__func__);
    auto&& data = parse_result(result);
    LOGV("%s", data.dump().c_str());
    if (data.empty()) {
        response(res, -1, result);
        return API_STATUS_OK;
    }
    response(res, 0, STR_OK, data);
    return API_STATUS_OK;
}

api_status_t api_device::updateSystem(request_t req, response_t res)
{
    script(__func__);
    response(res);
    return API_STATUS_OK;
}
