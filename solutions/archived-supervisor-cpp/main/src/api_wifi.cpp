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

#include "api_wifi.h"

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

json api_wifi::get_eth()
{
    auto&& e = parse_result(script(__func__));
    std::string ip = e.value("ip", "");
    if (ip.find("169.254") == 0) {
        ip = "";
    }
    e["ipAssignment"] = 1; // static / dhcp
    e["status"] = ip.empty() ? 1 : 3; // 1=not connected, 2=connecting, 3=connected
    return e;
}

json api_wifi::get_sta_current()
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
    c["auth"] = c.value("key_mgmt", "").find("WPA") != std::string::npos ? 1 : 0;
    c["macAddress"] = c.value("address", "");
    c["ip"] = ip;
    c["ipAssignment"] = 1; // static/dhcp
    c["subnetMask"] = "255.255.255.0";

    _wifi_mutex.lock();
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
    _wifi_mutex.unlock();

    return c;
}

json api_wifi::get_sta_connected(json& current)
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

void api_wifi::start_wifi()
{
    auto&& conf = parse_result(script(__func__));
    _sta_enable = conf.value("sta", 1);
    _ap_enable = conf.value("ap", 1);
    LOGV("sta_enable: %d, ap_enable: %d", _sta_enable, _ap_enable);

    int sta = 2; // no wifi
    if (_sta_enable != -1)
        sta = _sta_enable;
    _nw_info["wifiEnable"] = sta;

    _worker = std::thread([&]() {
        uint8_t timeout = 10;

        _need_scan = true;
        while (_running) {
            if (_need_scan || (_ap_enable == 1)) {
                _need_scan = false;
                auto&& e = get_eth();
                auto&& c = get_sta_current();
                auto&& n = get_sta_connected(c);
                auto&& l = get_scan_list(n);

                if ((e.value("status", 0) == 3) || (c.value("status", 0) == 3)) {
                    if (_ap_enable == 1) {
                        script("_ap_stop");
                        _ap_enable = 0;
                        timeout = 0;
                    }
                }

                _wifi_mutex.lock();
                _nw_info["etherInfo"] = e;
                _nw_info["connectedWifiInfoList"] = n;
                if (!l.empty()) {
                    _nw_info["wifiInfoList"] = l;
                }
                _wifi_mutex.unlock();
            }

            std::unique_lock<std::mutex> lock(_wifi_mutex);
            if (timeout == 0) {
                _cv.wait(lock, [] { return !_running || _need_scan; });
            } else {
                _cv.wait_for(lock, std::chrono::seconds(timeout), [] { return !_running || _need_scan; });
            }
        }
        LOGV("network thread exit");
    });
}

void api_wifi::stop_wifi()
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

api_status_t api_wifi::_wifi_ctrl(request_t req, response_t res, std::string ctrl)
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

// APIs
api_status_t api_wifi::connectWiFi(request_t req, response_t res)
{
    auto&& body = parse_body(req);
    std::string ssid = body.value("ssid", "");
    if (ssid.empty()) {
        response(res, -1, STR_FAILED);
        return API_STATUS_OK;
    }

    std::string msg = STR_OK;
    int id = -1;
    _wifi_mutex.lock();
    if (_nw_info.value("Selected", "") != ssid) {
        auto& n = _nw_info["connectedWifiInfoList"];
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
    _wifi_mutex.unlock();

    if (!ssid.empty()) {
        script(__func__, id, ssid, body.value("password", ""));
    }
    trigger_scan();
    response(res, 0, msg);
    return API_STATUS_OK;
}

api_status_t api_wifi::disconnectWiFi(request_t req, response_t res)
{
    return _wifi_ctrl(req, res, __func__);
}

api_status_t api_wifi::forgetWiFi(request_t req, response_t res)
{
    return _wifi_ctrl(req, res, __func__);
}

static json _parse_iw_scan(std::string fname)
{
    json jlist = json::array();
    std::ifstream f(fname);
    if (f.is_open()) {
        json j = json::object();
        std::string line;
        while (std::getline(f, line)) {
            if (line.find("BSS ") == 0) {
                if (!j.empty() && !j.value("ssid", "").empty())
                    jlist.push_back(j);
                j = json::object();
                j["bssid"] = line.substr(4, 17);
            } else if (line.find("\tfreq: ") == 0) {
                j["frequency"] = stoi(line.substr(7));
            } else if (line.find("\tsignal: ") == 0) {
                std::string sub = line.substr(9);
                j["signal"] = std::stod(sub.substr(0, sub.find(" dBm")));
            } else if (line.find("\tSSID: ") == 0) {
                std::string ssid = line.substr(7);
                if (ssid.empty())
                    continue;
                j["ssid"] = parse_escaped_string(ssid);
            } else if (line.find("\tRSN:\t") == 0) {
                j["rsn"] = line.substr(6);
            }
        }
        if (!j.empty() && !j.value("ssid", "").empty()) {
            jlist.push_back(j);
        }
    }
    return jlist;
}

json api_wifi::get_scan_list(json& connected)
{
    std::map<std::string, json> m;
    for (auto& j : _parse_iw_scan(script(__func__))) {
        // Compatible with previous
        j["auth"] = j.value("rsn", "").empty() ? 0 : 1;
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

api_status_t api_wifi::getWiFiInfoList(request_t req, response_t res)
{
    std::lock_guard<std::mutex> lock(_wifi_mutex);
    response(res, 0, STR_OK, _nw_info);
    trigger_scan();
    return API_STATUS_OK;
}

api_status_t api_wifi::switchWiFi(request_t req, response_t res)
{
    if (_sta_enable == -1) {
        response(res, -1, "wifi not supported");
    } else {
        _sta_enable = parse_body(req).value("mode", _sta_enable);
        _ap_enable = _sta_enable;
        script(__func__, _sta_enable);
        response(res, 0, STR_OK);
    }
    int sta = 2; // 0=disabled 1=enabled 2=no wifi
    if (_sta_enable != -1)
        sta = _sta_enable;
    _nw_info["wifiEnable"] = sta;
    return API_STATUS_OK;
}