#include <algorithm>
#include <chrono>
#include <cstddef>
#include <iomanip>
#include <iterator>
#include <map>
#include <random>
#include <sstream>
#include <utility>
#include <vector>

#include <iostream>
#include <string>

#include "api_halow.h"

static std::string parse_escaped_string(const std::string& input)
{
    std::string result;
    size_t i = 0;
    while (i < input.length()) {
        if (i + 3 < input.length() && input[i] == '\\' && input[i + 1] == 'x') {
            // 提取 \xHH 格式的两个十六进制字符
            std::string hex = input.substr(i + 2, 2);
            char byte = static_cast<char>(std::stoi(hex, nullptr, 16));
            if (byte != 0x00)
                result += byte;
            i += 4; // 跳过 \xHH
        } else {
            result += input[i];
            i++;
        }
    }
    return result;
}

json api_halow::get_halow_current()
{
    json c = json::object();
    for (auto& line : parse_result(script(__func__), '=', false)) {
        if (line.size() < 2)
            continue;
        c[line[0]] = line[1];
    }

    std::string ip = c.value("ip_address", "");
    if (ip.find("169.254") == 0) {
        ip = "";
    }

    std::string ssid = parse_escaped_string(c.value("ssid", ""));
    c["auth"] = c.value("key_mgmt", "").find("SAE") != std::string::npos ? 1 : 0;
    c["macAddress"] = c.value("address", "");
    c["ip"] = ip;
    c["ipAssignment"] = 1; // static/dhcp
    c["subnetMask"] = "255.255.255.0";

    _halow_mutex.lock();
    int status = 2; // 1=not connected, 2=connecting, 3=connected
    if (!ssid.empty() && !ip.empty() && c.value("wpa_state", "") == "COMPLETED") {
        status = 3;
    } else {
        if (_failed_cnt > 0) {
            _failed_cnt--;
        }
        if (_failed_cnt <= 0) {
            status = 1;
        }
    }
    if ((status == 1) || (status == 3)) {
        _nw_info["Selected"] = "";
    }
    c["ssid"] = (ssid.empty() ? _nw_info.value("Selected", "") : ssid);
    c["status"] = status;
    _halow_mutex.unlock();

    return c;
}

json api_halow::get_halow_connected(json& current)
{
    json n = json::array();
    if (!current.value("ssid", "").empty()) {
        n.push_back(current);
    }
    for (auto& line : parse_result(script(__func__), '\t', true)) {
        if (line.size() < 3 || line[1].empty())
            continue;
        std::string ssid = parse_escaped_string(line[1]);
        json j = { { "id", line[0] },
            { "ssid", ssid },
            { "bssid", line[2] },
            { "flags", line.size() > 3 ? line[3] : "" } };
        if (current.value("ssid", "") == ssid) {
            n[0].update(j);
            continue;
        }
        n.push_back(j);
    }
    return n;
}

void api_halow::start_halow()
{
    auto&& conf = parse_result(script(__func__));
    _sta_enable = conf.value("halow", 1);
    _antennaMode = !conf.value("antenna", 1);
    LOGV("halow_enable: %d", _sta_enable);

    int sta = 2; // no halow
    if (_sta_enable != -1)
        sta = _sta_enable;
    _nw_info["halowEnable"] = sta;
    _nw_info["antennaEnable"] = _antennaMode;

    _worker = std::thread([&]() {
        uint8_t timeout = 10;

        _need_scan = true;
        while (_running) {
            if (_need_scan) {
                _need_scan = false;
                auto&& c = get_halow_current();
                auto&& n = get_halow_connected(c);
                auto&& l = get_halow_scan_list(n);

                _halow_mutex.lock();
                _nw_info["connectedHalowInfoList"] = n;
				_nw_info["halowInfoList"] = l;
                _halow_mutex.unlock();
            }

            std::unique_lock<std::mutex> lock(_halow_mutex);
            if (timeout == 0) {
                _cv.wait(lock, [] { return !_running || _need_scan; });
            } else {
                _cv.wait_for(lock, std::chrono::seconds(timeout), [] { return !_running || _need_scan; });
            }
        }
        LOGV("network thread exit");
    });
}

void api_halow::stop_halow()
{
    LOGV("");
    if (_running) {
        _running = false;
        _cv.notify_all();
        if (_worker.joinable())
            _worker.join();
    }
    LOGV("exited");
}

api_status_t api_halow::_halow_ctrl(request_t req, response_t res, std::string ctrl)
{
    auto&& body = parse_body(req);
    std::string ssid = body.value("ssid", "");
    if (ssid.empty()) {
        response(res, -1, STR_FAILED);
        return API_STATUS_OK;
    }

    auto result = script(ctrl, ssid);
    response(res, result == STR_OK ? 0 : -1, result);
    _failed_cnt = 10;
    trigger_scan();
    return API_STATUS_OK;
}

api_status_t api_halow::connectHalow(request_t req, response_t res)
{
    int mode = -1;
    std::string ssid, country, encryption;
    auto&& body = parse_body(req);

    if (body.contains("halowInfo")) {
        country = body["halowInfo"].value("country", "");
        encryption = body["halowInfo"].value("encryption", "");
        mode = body["halowInfo"].value("mode", 0);
    }

    LOGV("country: %s, encryption: %s, mode: %d\n", country.c_str(), encryption.c_str(), mode);

    ssid = body.value("ssid", "");
    if (ssid.empty()) {
        response(res, -1, STR_FAILED);
        return API_STATUS_OK;
    }

    std::string msg = STR_OK;
    int id = -1;
    _halow_mutex.lock();
    if (_nw_info.value("Selected", "") != ssid) {
        auto& n = _nw_info["connectedHalowInfoList"];
        for (auto& _n : n) {
            if (_n.value("ssid", "") == ssid) {
                id = stoi(_n.value("id", "-1"));
                break;
            }
        }
        _nw_info["Selected"] = ssid;
        _failed_cnt = 10;
    } else {
        ssid = "";
        msg = "Connecting...";
    }
    _halow_mutex.unlock();

	if (id == -1 && (country.empty() || encryption.empty() || mode == -1)) {
        response(res, -1, STR_FAILED);
        return API_STATUS_OK;
	}

    if (!ssid.empty()) {
        script(__func__, id, ssid, body.value("password", ""), country, encryption, mode);
    }
    trigger_scan();
    response(res, 0, msg);
    return API_STATUS_OK;
}

api_status_t api_halow::disconnectHalow(request_t req, response_t res)
{
    return _halow_ctrl(req, res, __func__);
}

api_status_t api_halow::forgetHalow(request_t req, response_t res)
{
    return _halow_ctrl(req, res, __func__);
}

static json _parse_halow_scan(std::string fname)
{
    json jlist = json::array();
    std::ifstream f(fname);
    if (f.is_open()) {
        std::string line;
        json j;
        std::getline(f, line); // skip first line
        while (std::getline(f, line)) {
            std::stringstream ss(line);
            std::string field;
            j = json::object();
            int cnt = 0;
            while (std::getline(ss, field, '\t')) {
                switch (cnt++) {
                    case 0:
                        j["bssid"] = field;
                        break;
                    case 1:
                        j["frequency"] = stoi(field);
                        break;
                    case 2:
                        j["signal"] = std::stod(field);
                        break;
                    case 3:
                        j["rsn"] = field;
                        break;
                    case 4:
                        j["ssid"] = parse_escaped_string(field);
                        break;
                }
            }
            if (!j.empty() && !j.value("ssid", "").empty()) {
                jlist.push_back(j);
            }
        }
        if (!j.empty() && !j.value("ssid", "").empty()) {
            jlist.push_back(j);
        }
    }
    return jlist;
}

json api_halow::get_halow_scan_list(json& connected)
{
    std::map<std::string, json> m;
    for (auto& j : _parse_halow_scan(script(__func__))) {
        // Compatible with previous
        j["auth"] = j.value("rsn", "").find("SAE") != std::string::npos ? 1 : 0;
        j["macAddress"] = j["bssid"];
        m[j["ssid"]] = j;
    }

    // sort by signal strength
    std::vector<std::string> list;
    std::transform(m.begin(), m.end(), std::back_inserter(list), [](const auto& p) { return p.first; });
    std::sort(list.begin(), list.end(), [&](const auto& a, const auto& b) {
        return m[a]["signal"] > m[b]["signal"];
    });

    json result = json::array();
    for (auto& ssid : list) {
        bool found = false;
        for (auto& j : connected) {
            if (j.value("ssid", "") == ssid) {
                j.update(m[ssid]);
                found = true;
                break;
            }
        }
        if (!found)
            result.push_back(std::move(m[ssid]));
    }
    return result;
}

api_status_t api_halow::getHalowInfoList(request_t req, response_t res)
{
    std::lock_guard<std::mutex> lock(_halow_mutex);
    response(res, 0, STR_OK, _nw_info);
    trigger_scan();
    return API_STATUS_OK;
}

api_status_t api_halow::switchHalow(request_t req, response_t res)
{
    if (_sta_enable == -1) {
        response(res, -1, "halow not supported");
    } else {
        _sta_enable = parse_body(req).value("mode", _sta_enable);
        script(__func__, _sta_enable);
        response(res, 0, STR_OK);
    }
    int sta = 2; // 0=disabled 1=enabled 2=no halow
    if (_sta_enable != -1)
        sta = _sta_enable;
    _nw_info["halowEnable"] = sta;
    return API_STATUS_OK;
}

api_status_t api_halow::switchAntenna(request_t req, response_t res)
{
    _antennaMode = parse_body(req).value("mode", _antennaMode);
    script(__func__, !_antennaMode);
    response(res, 0, STR_OK);
    // 0 = RF1 1 = RF2
    _nw_info["antennaEnable"] = _antennaMode;
    return API_STATUS_OK;
}

void api_halow::start_ping(const std::string& ip, int interval)
{
    stop_ping(); // Stop existing if any

    _ping_ip = ip;
    _ping_interval = interval;
    _ping_running = true;

    _ping_worker = std::thread([]() {
        LOGI("Ping task started: ip=%s, interval=%d", _ping_ip.c_str(), _ping_interval.load());
        while (_ping_running) {
            std::string cmd = "ping -I halow0 -c 1 -W 1 " + _ping_ip + " >/dev/null 2>&1";
            int ret = system(cmd.c_str());
            // LOGD("Ping %s result: %d", _ping_ip.c_str(), ret);

            std::unique_lock<std::mutex> lock(_ping_mutex);
            if (_ping_cv.wait_for(lock, std::chrono::seconds(_ping_interval), [] { return !_ping_running; })) {
                break;
            }
        }
        LOGI("Ping task stopped");
    });
    _ping_worker.detach();
}

void api_halow::stop_ping()
{
    if (_ping_running) {
        _ping_running = false;
        _ping_cv.notify_all();
        if (_ping_worker.joinable()) {
            _ping_worker.join();
        }
    }
}

api_status_t api_halow::startPing(request_t req, response_t res)
{
    auto&& body = parse_body(req);
    std::string ip = body.value("ip", "");
    int interval = body.value("interval", 5);

    if (interval < 1) interval = 1;

    if (ip.empty()) {
        json c = json::object();
        for (auto& line : parse_result(script("get_halow_current"), '=', false)) {
            if (line.size() < 2) continue;
            c[line[0]] = line[1];
        }
        std::string my_ip = c.value("ip_address", "");

        if (my_ip.empty() || my_ip.find("169.254") == 0) {
            response(res, -1, "Halow not connected or no valid IP");
            return API_STATUS_OK;
        }

        size_t last_dot = my_ip.find_last_of('.');
        if (last_dot != std::string::npos) {
            ip = my_ip.substr(0, last_dot) + ".1";
        } else {
            response(res, -1, "Invalid IP format");
            return API_STATUS_OK;
        }
        LOGI("Auto-detected ping target: %s (from %s)", ip.c_str(), my_ip.c_str());
    }

    start_ping(ip, interval);

    json data = json::object();
    data["pingIp"] = _ping_ip;
    data["pingInterval"] = _ping_interval.load();
    data["pingEnabled"] = _ping_running.load();

    response(res, 0, STR_OK, data);
    return API_STATUS_OK;
}

api_status_t api_halow::stopPing(request_t req, response_t res)
{
    stop_ping();

    json data = json::object();
    data["pingEnabled"] = false;

    response(res, 0, STR_OK, data);
    return API_STATUS_OK;
}

api_status_t api_halow::getPingStatus(request_t req, response_t res)
{
    json data = json::object();
    data["pingEnabled"] = _ping_running.load();
    data["pingIp"] = _ping_ip;
    data["pingInterval"] = _ping_interval.load();

    response(res, 0, STR_OK, data);
    return API_STATUS_OK;
}
