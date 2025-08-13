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
    auto&& r = parse_result(script(__func__));
    std::string ip = r.value("ip", "");
    if (ip.find("169.254") != std::string::npos) {
        ip = "";
    }

    // Don't be strange: Compatible with previous
    json e = json::object();
    e["connectedStatus"] = (int)_eth_status;
    e["ip"] = ip;
    e["macAddress"] = r.value("mac", "");
    e["subnetMask"] = r.value("mask", "");
    e["gateway"] = r.value("gateway", "");
    e["dns1"] = r.value("dns1", "");
    e["dns2"] = r.value("dns2", "");
    e["ipAssignment"] = 1; // static / dhcp
    _eth = e;
    // LOGV("%s", e.dump().c_str());
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
    c["ssid"] = parse_escaped_string(c.value("ssid", ""));

    std::string ip = c.value("ip_address", "");
    if (ip.find("169.254") != std::string::npos) {
        ip = "";
        c["ip_address"] = "";
    }

    // Don't be strange: Compatible with previous
    c["auth"] = c.value("key_mgmt", "").find("WPA") != std::string::npos ? 1 : 0;
    c["macAddress"] = c.value("address", "");
    c["ip"] = ip;
    c["ipAssignment"] = 1; // static/dhcp
    c["subnetMask"] = "255.255.255.0";
    _sta_current = c;
    // LOGV("%s", c.dump().c_str());
    return c;
}

json api_wifi::get_sta_connected()
{
    auto&& c = _sta_current;
    json n = json::array();
    if (!c.empty() && !c.value("ssid", "").empty()) {
        n.push_back(c);
    }
    for (auto& line : parse_result(script(__func__), '\t', true)) {
        if (line.size() < 3 || line[1].empty())
            continue;
        std::string ssid = parse_escaped_string(line[1]);
        json j = { { "id", line[0] },
            { "ssid", ssid },
            { "bssid", line[2] },
            { "flags", line.size() > 3 ? line[3] : "" } };
        if (!c.empty() && (ssid == c.value("ssid", ""))) {
            c.update(j);
            n[0] = c;
        } else {
            n.push_back(j);
        }
    }

    // Don't be strange: Compatible with previous
    for (auto& j : n) {
        j["connectedStatus"] = 1;
        j["autoConnect"] = j.value("flags", "").find("DISABLED") != std::string::npos ? 0 : 1;
    }
    // LOGV("%s", n.dump().c_str());
    return n;
}

void api_wifi::start_wifi()
{
    auto&& conf = parse_result(script(__func__));
    _sta_enable = conf.value("sta", 1);
    _ap_enable = conf.value("ap", 1);
    LOGV("sta_enable: %d, ap_enable: %d", _sta_enable, _ap_enable);

    _worker = std::thread([&]() {
        uint32_t stable_count = 0;
        uint8_t timeout;

        while (_running) {
            timeout = 15;
            // eth
            get_eth();
            _eth_status = !_eth.value("ip", "").empty();

            // sta
            if (_sta_enable < 0) {
                _sta_status = 4; // wifi not supported
            } else if (_sta_enable == 0) {
                _sta_status = 0; // wifi disabled
            } else {
                auto&& c = get_sta_current();
                _sta_status = 2; // connecting
                if (!c.value("ssid", "").empty() && !c.value("ip", "").empty()
                    && c.value("wpa_state", "") == "COMPLETED") {
                    _sta_status = 3; // connected
                }
                timeout = 3;
            }
            if ((_eth_status == 1) || (_sta_status == 3)) {
                if (stable_count < 10) {
                    stable_count++;
                } else {
                    script("_ap_stop");
                }
            } else {
                stable_count = 0;
            }

            if (_need_scan) {
                _get_scan_results();
                _need_scan = false;
            }

            std::unique_lock<std::mutex> lock(wifi_mutex);
            cv.wait_for(lock, std::chrono::seconds(timeout), [] { return !_running; });
        }
        LOGV("network thread exit");
    });
}

void api_wifi::stop_wifi()
{
    LOGV("");
    if (_running) {
        _running = false;
        cv.notify_all();
        if (_worker.joinable())
            _worker.join();
    }
    // script(__func__);
    LOGV("exited");
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

    int id = -1;
    auto&& n = get_sta_connected();
    for (auto& _n : n) {
        if (_n.value("ssid", "") == ssid) {
            id = stoi(_n.value("id", "-1"));
        }
    }
    script(__func__, id, ssid, body.value("password", ""));

    // Todo: Stop waiting
    int i = 60;
    _sta_status = 2; // connecting
    while (_running && i--) {
        if (_sta_status == 3)
            break;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    if (i <= 0) {
        script("forgetWiFi", ssid);
        response(res, -1, "Connect failed");
    } else {
        response(res, 0, STR_OK);
    }
    return API_STATUS_OK;
}

api_status_t api_wifi::scanWiFi(request_t req, response_t res)
{ // Don't be strange: Compatible with previous
    response(res, 0, STR_OK);
    return API_STATUS_OK;
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

void api_wifi::_get_scan_results()
{
    auto&& n = get_sta_connected();
    std::map<std::string, json> m;
    for (auto& j : _parse_iw_scan(script(__func__))) {
        // Compatible with previous
        j["auth"] = j.value("rsn", "").empty() ? 0 : 1;
        j["macAddress"] = j["bssid"];
        j["connectedStatus"] = 0;
        j["autoConnect"] = 0;

        bool found = false;
        for (auto& _n : n) {
            if (_n.value("ssid", "") == j["ssid"]) {
                j.update(_n);
                _n = j;
                found = true;
                break;
            }
        }
        if (!found)
            m[j["ssid"]] = j;
    }

    // sort by signal strength
    std::vector<std::string> list;
    std::transform(m.begin(), m.end(), std::back_inserter(list), [](const auto& p) { return p.first; });
    std::sort(list.begin(), list.end(), [&](const auto& a, const auto& b) {
        return m[a]["signal"] > m[b]["signal"];
    });

    json& data = _scan_results;
    data["wifiInfoList"] = json::array();
    for (auto& _n : n) {
        data["wifiInfoList"].push_back(_n);
    }
    for (auto& l : list) {
        data["wifiInfoList"].push_back(m[l]);
    }

    data["etherinfo"] = _eth;
}

api_status_t api_wifi::getWiFiScanResults(request_t req, response_t res)
{
    _need_scan = true;
    response(res, 0, STR_OK, _scan_results);
    return API_STATUS_OK;
}

api_status_t api_wifi::getWifiStatus(request_t req, response_t res)
{ // Don't be strange: Compatible with previous
    int status = 2; // wifi not connected
    if (_eth_status == 1) {
        status = 0; // eth connected
    } else if (_sta_status == 3) {
        status = 1; // wifi connected
    }
    json data = json::object();
    data["status"] = status;
    response(res, 0, STR_OK, data);
    return API_STATUS_OK;
}

api_status_t api_wifi::queryWiFiInfo(request_t req, response_t res)
{
    json data = json::object();
    int status = _sta_status;
    data["status"] = status;
    if (status == 3) {
        data["wifiInfo"] = _sta_current;
    }
    response(res, 0, STR_OK, data);
    return API_STATUS_OK;
}

api_status_t api_wifi::switchWiFi(request_t req, response_t res)
{
    if (_sta_enable == -1) {
        response(res, -1, "wifi not supported");
    } else {
        _sta_enable = parse_body(req).value("mode", _sta_enable);
        _sta_status = (_sta_enable) ? 2 : 0;
        script(__func__, _sta_enable);
        response(res, 0, STR_OK);
    }
    return API_STATUS_OK;
}
