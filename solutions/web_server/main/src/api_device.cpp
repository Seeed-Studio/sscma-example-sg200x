#include "api_device.h"
#include "api_file.h"

api_status_t api_device::getCameraWebsocketUrl(request_t req, response_t res)
{
    auto&& body = parse_body(req);

    int64_t itime = 0;
    if (body.contains("time")) {
        itime = body.value("time", 0);
        itime /= 1000;
    }

    std::stringstream ss;
    ss << "ws://" << get_host(req) << ":" << script(__func__, itime);
    response(res, 0, STR_OK, { { "websocketUrl", ss.str() } });
    return API_STATUS_OK;
}

api_status_t api_device::getDeviceInfo(request_t req, response_t res)
{
    auto&& data = json::parse(script(__func__));
    data["ip"] = get_host(req);
    data["port"] = get_port(req);
    response(res, 0, STR_OK, data);
    return API_STATUS_OK;
}

api_status_t api_device::getDeviceList(request_t req, response_t res)
{
    auto&& result = json::parse(script(__func__));
    std::ifstream file(result.value("file", ""));
    if (!file.is_open()) {
        throw std::runtime_error(__func__);
    }

    using namespace std;
    map<pair<string, string>, map<string, string>> device_map;
    json dev_list;
    std::string line;
    while (getline(file, line)) {
        if (line.empty() || line[0] != '=')
            continue;

        std::vector<std::string> it;
        string_split(line, ';', it);
        std::string type = it[1];
        std::string service = it[4];
        std::string ip = it[7];
        std::string port = it[8];
        if ((service == "_sscma._tcp")
            && (std::string(it[9]).find("sn=") != std::string::npos)) {
            dev_list.push_back({ { "type", type },
                { "device", it[3] },
                { "domain", it[6] },
                { "ip", ip },
                { "info", it[9] } });
        }
        device_map[std::make_pair(type, ip)][service] = port;
    }

    for (auto& dev : dev_list) {
        auto it = device_map.find(std::make_pair(dev["type"], dev["ip"]));
        if (it != device_map.end()) {
            for (auto& [service, port] : it->second) {
                dev["services"][service] = port;
            }
        }
    }
    file.close();
    response(res, 0, STR_OK, { { "deviceList", dev_list } });
    return API_STATUS_OK;
}

api_status_t api_device::getPlatformInfo(request_t req, response_t res)
{
    json data;
    std::ifstream(script(__func__)) >> data["platform_info"];
    response(res, 0, STR_OK, data);
    return API_STATUS_OK;
}

api_status_t api_device::getSystemStatus(request_t req, response_t res)
{
    auto&& result = script(__func__);
    response(res, (result == STR_OK) ? 0 : -1, result);
    return API_STATUS_OK;
}

api_status_t api_device::queryDeviceInfo(request_t req, response_t res)
{
    auto&& data = json::parse(script(__func__));
    data["appName"] = PROJECT_NAME;
    data["ip"] = get_host(req);
    data["port"] = get_port(req);
    response(res, 0, STR_OK, data);
    return API_STATUS_OK;
}

api_status_t api_device::queryServiceStatus(request_t req, response_t res)
{
    auto&& data = json::parse(script(__func__));
    data["uptime"] = uptime();
    data["timestamp"] = timestamp();
    response(res, 0, STR_OK, data);
    return API_STATUS_OK;
}

api_status_t api_device::savePlatformInfo(request_t req, response_t res)
{
    auto&& body = parse_body(req);
    std::ofstream(script(__func__)) << body.value("platform_info", "");
    response(res, 0, STR_OK);
    return API_STATUS_OK;
}

api_status_t api_device::setPower(request_t req, response_t res)
{
    auto&& body = parse_body(req);
    std::string result = script(__func__, body.value("mode", -1));
    if (result == STR_FAILED)
        throw std::runtime_error("Failed: " + std::string(__func__) + " result: " + result);
    response(res, 0, result);
    return API_STATUS_OK;
}

api_status_t api_device::updateChannel(request_t req, response_t res)
{
    auto&& body = parse_body(req);

    const auto& ch = body.value("channel", 0);
    const auto& url = body.value("serverUrl", "");
    if (ch > 0) {
        if (url.empty()) {
            response(res, -1, "Server url is empty.");
            return API_STATUS_OK;
        }
        if (url.compare(0, 7, "http://") != 0
            && url.compare(0, 8, "https://") != 0) {
            response(res, -1, "Server url is invalid.");
            return API_STATUS_OK;
        }
    }
    response(res, 0, script(__func__, ch, url));
    return API_STATUS_OK;
}

api_status_t api_device::updateDeviceName(request_t req, response_t res)
{
    auto&& body = parse_body(req);
    std::string result = script(__func__, body.value("deviceName", ""));
    if (result != STR_OK)
        throw std::runtime_error("Failed: " + std::string(__func__) + " result: " + result);
    response(res, 0, result);
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
    response(res, 0, STR_OK, json::parse(script(__func__)));
    return API_STATUS_REPLY_FILE;
}

api_status_t api_device::getModelInfo(request_t req, response_t res)
{
    auto&& data = json::parse(script(__func__));
    std::ifstream(data.value("file", "")) >> data["model_info"];
    response(res, 0, STR_OK, data);
    return API_STATUS_OK;
}

api_status_t api_device::getModelList(request_t req, response_t res)
{
    auto&& list = json::parse(script(__func__));
    if (list.empty())
        throw std::runtime_error(__func__);

    const auto& suffix = list.value("suffix", "");
    const auto& count = list.value("count", 0);

    json data;
    data["count"] = count;
    for (auto&& item : list["list"]) {
        if (auto fname = item.get<std::string>(); !fname.empty()) {
            json info = json::parse(std::ifstream(fname + ".json"));
            info["id"] = info.value("model_id", "");
            info["name"] = info.value("model_name", "");
            info["md5"] = info.value("checksum", "");
            info["file"] = fname + suffix;
            data["list"].push_back(info);
        }
    }
    response(res, 0, STR_OK, data);
    return API_STATUS_OK;
}

api_status_t api_device::uploadModel(request_t req, response_t res)
{
    std::string dir = script(__func__);
    if (dir.empty()) {
        response(res, -1, "Directory is not accessible.");
        return API_STATUS_OK;
    }

    int count = 0;
    json data;
    auto&& parts = get_multiparts(req, "model_file");
    for (auto& part : parts) {
        if (part.filename.empty() || part.len == 0) {
            continue;
        }

        std::ofstream file(dir + "/" + part.filename, std::ios::binary);
        if (!file.is_open()) {
            continue;
        }
        file.write(part.data, part.len);
        file.close();
        data["list"].push_back(part.filename);
        count++;
    }
    data["count"] = count;

    MA_LOGV(data.dump(4));
    response(res, 0, STR_OK, data);
    return API_STATUS_OK;
}

// Upgrade
api_status_t api_device::cancelUpdate(request_t req, response_t res)
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

api_status_t api_device::updateSystem(request_t req, response_t res)
{
    MA_LOGV("");

    res["code"] = 0;
    res["msg"] = "";
    res["data"] = json({});

    return API_STATUS_OK;
}
