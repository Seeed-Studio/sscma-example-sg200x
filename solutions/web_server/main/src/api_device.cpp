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
    auto&& data = parse_result(script(__func__));
    data["ip"] = get_host(req);
    data["port"] = get_port(req);
    response(res, 0, STR_OK, data);
    return API_STATUS_OK;
}

api_status_t api_device::getDeviceList(request_t req, response_t res)
{
    auto&& result = parse_result(script(__func__));
    std::ifstream file(result.value("file", ""));
    if (!file.is_open()) {
        response(res, 0, STR_OK, { { "deviceList", "" } });
        return API_STATUS_OK;
    }

    json dev_list;
    using namespace std;
    map<pair<string, string>, map<string, string>> device_map;
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
    std::string fname = script(__func__);
    if (fname.empty())
        data["platform_info"] = "";
    else
        std::ifstream(fname) >> data["platform_info"];
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
    auto&& data = parse_result(script(__func__));
    data["appName"] = PROJECT_NAME;
    data["ip"] = get_host(req);
    data["port"] = get_port(req);
    response(res, 0, STR_OK, data);
    return API_STATUS_OK;
}

api_status_t api_device::queryServiceStatus(request_t req, response_t res)
{
    auto&& data = parse_result(script(__func__));
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
    response(res, (result == STR_OK) ? 0 : -1, result);
    return API_STATUS_OK;
}

api_status_t api_device::updateChannel(request_t req, response_t res)
{
    auto&& body = parse_body(req);
    const auto& ch = body.value("channel", 0);
    const auto& url = body.value("serverUrl", "");
    if (ch > 0) {
        if (url.empty()) {
            response(res, 0, script(__func__, ch, url));
            response(res);
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
    response(res, (result == STR_OK) ? 0 : -1, result);
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
    auto&& data = parse_result(script(__func__));
    response(res, 0, STR_OK, data);
    return API_STATUS_REPLY_FILE;
}

api_status_t api_device::getModelInfo(request_t req, response_t res)
{
    auto&& data = parse_result(script(__func__));
    std::ifstream file(data.value("file", ""));
    if (!file.is_open()) {
        response(res, 0, STR_OK, { { "model_info", "" } });
        return API_STATUS_OK;
    }
    file >> data["model_info"];
    response(res, 0, STR_OK, data);
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
    const auto& count = list.value("count", 0);

    json data;
    data["count"] = count;
    for (auto&& item : list["list"]) { // Compatible with previous
        if (auto fname = item.get<std::string>(); !fname.empty()) {
            auto&& info = parse_result(std::ifstream(fname + ".json"));
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

    LOGV("Upload model file: %s", data.dump(4).c_str());
    response(res, 0, STR_OK, data);
    return API_STATUS_OK;
}

// Upgrade
api_status_t api_device::cancelUpdate(request_t req, response_t res)
{
    script(__func__);
    response(res);
    return API_STATUS_OK;
}

api_status_t api_device::getSystemUpdateVesionInfo(request_t req, response_t res)
{ // typo
    auto result = script(__func__);
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
    auto result = script(__func__);
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
