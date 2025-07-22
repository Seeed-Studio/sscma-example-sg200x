#include <algorithm>
#include <cstddef>
#include <iomanip>
#include <iterator>
#include <map>
#include <random>
#include <sstream>
#include <utility>
#include <vector>

#include "api_wifi.h"

json api_wifi::get_networks()
{
    json n = json::array();
    for (auto& line : parse_file(script(__func__), '\t', true)) {
        if (line.size() < 4)
            continue;
        n.push_back({ { "id", line[0] },
            { "ssid", line[1] },
            { "bssid", line[2] },
            { "flags", line[3] } });
    }

    // Don't be strange: Compatible with previous
    for (auto& j : n) {
        j["connectedStatus"] = 1;
        j["autoConnect"] = j.value("flags", "").find("DISABLED") != std::string::npos ? 0 : 1;
    }
    // LOGV("%s", n.dump().c_str());
    return n;
}

json api_wifi::get_current()
{
    json c = json::object();
    for (auto& line : parse_file(script(__func__), '=', false)) {
        if (line.size() < 2)
            continue;
        c[line[0]] = line[1];
    }
    std::string ip = c.value("ip_address", "");
    if (ip.find("169.254") != std::string::npos) {
        ip = "";
    }

    // Don't be strange: Compatible with previous
    c["auth"] = c.value("key_mgmt", "").find("WPA") != std::string::npos ? 1 : 0;
    c["macAddress"] = c.value("address", "");
    c["ip"] = ip;
    c["ipAssignment"] = 1; // static/dhcp
    c["subnetMask"] = "255.255.255.0";
    // LOGV("%s", c.dump().c_str());
    return c;
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
    e["connectedStatus"] = ip.empty() ? 0 : 1;
    e["ip"] = ip;
    e["macAddress"] = r.value("mac", "");
    e["subnetMask"] = r.value("mask", "");
    e["gateway"] = r.value("gateway", "");
    e["dns1"] = r.value("dns1", "");
    e["dns2"] = r.value("dns2", "");
    e["ipAssignment"] = 1; // static / dhcp
    // LOGV("%s", e.dump().c_str());
    return e;
}

// APIs
api_status_t api_wifi::connectWiFi(request_t req, response_t res)
{
    // _wifi_ctrl(req, res, __func__);
    auto&& body = parse_body(req);
    std::string ssid = body.value("ssid", "");
    if (ssid.empty()) {
        response(res, -1, STR_FAILED);
        return API_STATUS_OK;
    }

    int cur = -1;
    int id = -1;
    auto&& n = get_networks();
    for (auto& _n : n) {
        if (_n.value("flags", "").find("CURRENT") != std::string::npos) {
            cur = stoi(_n.value("id", "-1"));
        }
        if (_n.value("ssid", "") == ssid) {
            id = stoi(_n.value("id", "-1"));
        }
    }
    script(__func__, cur, id, ssid, body.value("password", ""));

    int i = 60;
    while (i--) {
        auto&& c = get_current();
        if (!c.empty() && !c.value("ssid", "").empty()
            && !c.value("ip", "").empty()
            && c.value("wpa_state", "") == "COMPLETED") {
            break;
        }
        sleep(1);
    }
    if (i <= 0) {
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

api_status_t api_wifi::getWiFiScanResults(request_t req, response_t res)
{
    auto&& n = get_networks();
    auto&& c = get_current();

    std::string c_ssid = "";
    if (!c.empty() && !c.value("ssid", "").empty() && !c.value("ip", "").empty()) {
        auto it = std::find_if(n.begin(), n.end(), [&](const auto& item) {
            return item.value("ssid", "") == c.value("ssid", "");
        });
        if (it != n.end()) {
            auto new_it = *it;
            new_it.update(c);
            n.erase(it);
            n.insert(n.begin(), new_it);
        } else {
            n.insert(n.begin(), c);
        }
    }

    std::map<std::string, json> m;
    for (auto& l : parse_file(script(__func__), '\t', true)) {
        if ((l.size() < 5) || l[4].empty())
            continue;
        json j = json::object();
        j["bssid"] = l[0];
        j["frequency"] = stoi(l[1]);
        j["signal"] = stoi(l[2]);
        j["flags"] = l[3];
        j["ssid"] = l[4];

        // Compatible with previous
        j["auth"] = j.value("flags", "").find("WPA") != std::string::npos ? 1 : 0;
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
        if (!found) {
            m[j["ssid"]] = j; // ignore duplicate
        }
    }

    // sort by signal strength
    std::vector<std::string> list;
    std::transform(m.begin(), m.end(), std::back_inserter(list), [](const auto& p) { return p.first; });
    std::sort(list.begin(), list.end(), [&](const auto& a, const auto& b) {
        return m[a]["signal"] > m[b]["signal"];
    });

    json data = json::object();
    data["wifiInfoList"] = json::array();
    for (auto& _n : n) {
        if (!_n.value("ssid", "").empty()) {
            data["wifiInfoList"].push_back(_n);
        }
    }
    for (auto& l : list) {
        data["wifiInfoList"].push_back(m[l]);
    }

    data["etherinfo"] = get_eth();
    response(res, 0, STR_OK, data);
    return API_STATUS_OK;
}

api_status_t api_wifi::getWifiStatus(request_t req, response_t res)
{ // Don't be strange: Compatible with previous
    int status = 2; // wifi not connected
    auto&& eth = get_eth();
    if (!eth.value("ip", "").empty()) {
        status = 0; // eth connected
    } else {
        auto&& info = get_current();
        if (!info.value("ip", "").empty()) {
            status = 1; // wifi connected
        }
    }

    json data = json::object();
    data["status"] = status;
    response(res, 0, STR_OK, data);
    return API_STATUS_OK;
}

api_status_t api_wifi::queryWiFiInfo(request_t req, response_t res)
{
    json data = json::object();

    int status = 4; // wifi not supported
    if (_is_open == 0) {
        status = 0; // wifi disabled
    } else if (_is_open == 1) {
        auto&& c = get_current();
        status = 1; // not connected
        if (!c.empty()) {
            if (!c.value("ip", "").empty()) {
                status = 3; // connected
                data["wifiInfo"] = c;
            } else if (!c.value("wpa_state", "").empty()) {
                status = 2; // connecting
            }
        }
    }
    data["status"] = status;
    response(res, 0, STR_OK, data);
    return API_STATUS_OK;
}

api_status_t api_wifi::switchWiFi(request_t req, response_t res)
{
    if (_is_open == -1) {
        response(res, -1, "wifi not supported");
    } else {
        _is_open = parse_body(req).value("mode", _is_open);
        script(__func__, _is_open);
        response(res, 0, STR_OK);
    }
    return API_STATUS_OK;
}
