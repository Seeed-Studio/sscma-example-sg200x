#include <stdio.h>
#include <iostream>
#include <vector>
#include <map>
#include "HttpServer.h"
#include "app_ipc_wifi.h"

#include <string.h>

typedef struct _WIFI_INFO_S {
    int id;
    int connectedStatus;
    int autoConnect;
} WIFI_INFO_S;

std::vector<std::vector<std::string>> g_wifiList;
std::map<std::string, WIFI_INFO_S> g_wifiInfo;
std::string g_currentWifi;
int g_wifiMode = 0;

static int getWifiInfo(std::vector<std::string> &wifiStatus) {
    FILE *fp;
    char info[128];

    fp = popen("/mnt/wifitool.sh status", "r");
    if (fp == NULL) {
        printf("Failed to run /mnt/wifitool.sh status\n");
        return -1;
    }

    while (fgets(info, sizeof(info) - 1, fp) != NULL) {
        std::string s(info);
        if (s.back() == '\n') {
            s.erase(s.size() - 1);
        }
        wifiStatus.push_back(s);
    }

    pclose(fp);

    return 0;
}

static int getWifiList() {
    FILE *fp;
    char info[128];
    std::vector<std::vector<std::string>> wifiList;

    fp = popen("/mnt/wifitool.sh scan", "r");
    if (fp == NULL) {
        printf("Failed to run /mnt/wifitool.sh scan\n");
        return -1;
    }

    while (fgets(info, sizeof(info) - 1, fp) != NULL) {
        std::vector<std::string> wifi;

        char *token = strtok(info, " ");
        while (token != NULL) {
            wifi.push_back(std::string(token));
            token = strtok(NULL, " ");
        }

        if (!wifi.empty()) {
            // TODO:: Need to determine if wifi is encrypted or not
            wifi[2] = "1";
            wifiList.push_back(wifi);
        }
    }

    if (!wifiList.empty()) {
        g_wifiList = wifiList;
    }

    pclose(fp);
    return 0;
}

static int updateConnectedWifiInfo() {
    FILE *fp;
    char info[128];

    std::cout << "updateConnectedWifiInfo operation\n";

    fp = popen("/mnt/wifitool.sh list", "r");
    if (fp == NULL) {
        printf("Failed to run /mnt/wifitool.sh list\n");
        return -1;
    }

    while (fgets(info, sizeof(info) - 1, fp) != NULL) {
        std::vector<std::string> wifi;

        char *token = strtok(info, " ");
        while (token != NULL) {
            wifi.push_back(std::string(token));
            token = strtok(NULL, " ");
        }

        g_wifiInfo[wifi[1]].id = stoi(wifi[0]);
        g_wifiInfo[wifi[1]].connectedStatus = 1;
        g_wifiInfo[wifi[1]].autoConnect = 1;
    }

    pclose(fp);

    return 0;
}

int queryWiFiInfo(HttpRequest* req, HttpResponse* resp) {

    std::vector<std::string> wifiStatus;

    if (getWifiInfo(wifiStatus) != 0) {
        return -1;
    }

    hv::Json response;
    response["code"] = 0;
    response["msg"] = "";

    hv::Json data;
    data["status"] = 3;

    hv::Json wifiInfo;

    wifiInfo["ssid"] = wifiStatus[0];
    g_currentWifi = wifiStatus[0];
    if (wifiStatus[1] != "") {
        wifiInfo["auth"] = 1;
    } else {
        wifiInfo["auth"] = 0;
    }
    wifiInfo["signal"] = 0;
    wifiInfo["connectedStatus"] = 1;
    wifiInfo["macAddres"] = wifiStatus[2];
    wifiInfo["ip"] = wifiStatus[3];
    wifiInfo["ipAssignment"] = 0;
    wifiInfo["autoConnect"] = 0;

    data["wifiInfo"] = wifiInfo;
    response["data"] = data;

    return resp->Json(response);
}

int scanWiFi(HttpRequest* req, HttpResponse* resp) {

    std::cout << "\nscan WiFi operation...\n";
    std::cout << "scanTime: " << req->GetString("scanTime") << "\n";

    if (updateConnectedWifiInfo() != 0) {
        return -1;
    }

    if (getWifiList() != 0) {
        return -1;
    }

    hv::Json response;
    response["code"] = 0;
    response["msg"] = "";

    hv::Json data;
    hv::Json etherInfo;
    hv::Json wifiInfo;
    std::vector<hv::Json> wifiInfoList;

    std::cout << "current wifi: =" << g_currentWifi << "=\n";

    for (auto wifi: g_wifiList) {
        wifiInfo.clear();
        wifiInfo["ssid"] = wifi[0];
        wifiInfo["auth"] = stoi(wifi[2]);
        wifiInfo["signal"] = stoi(wifi[1]);

        if (g_wifiInfo.find(wifi[0]) != g_wifiInfo.end()) {
            wifiInfo["connectedStatus"] = g_wifiInfo[wifi[0]].connectedStatus;
        } else {
            wifiInfo["connectedStatus"] = 0;
        }

        wifiInfo["macAddress"] = wifi[3].substr(0, 17);
        wifiInfo["ip"] = "";
        wifiInfo["ipAssignment"] = "";
        wifiInfo["dns1"] = "";
        wifiInfo["dns2"] = "";
        wifiInfo["autoConnect"] = 0;
        wifiInfoList.push_back(wifiInfo);
    }

    etherInfo["connectedStatus"] = 0;
    etherInfo["macAddres"] = "-";
    etherInfo["ip"] = "-";
    etherInfo["ipAssignment"] = 0;
    etherInfo["dns1"] = "-";
    etherInfo["dns2"] = "-";
    data["etherinfo"] = etherInfo;

    data["wifiInfoList"] = hv::Json(wifiInfoList);
    response["data"] = data;

    // std::cout << "response: " << response.dump(2) << "\n";

    return resp->Json(response);
}

int connectWiFi(HttpRequest* req, HttpResponse* resp) {

    std::cout << "\nconnect WiFi...\n";
    std::cout << "ssid: " << req->GetString("ssid") << "\n";
    std::cout << "ssid: " << req->GetString("password") << "\n";

    std::string msg;
    int id;
    FILE *fp;
    char info[128];
    char cmd[128] = "/mnt/wifitool.sh ";

    if (req->GetString("password").empty()) {
        strcat(cmd, "select ");
        id = g_wifiInfo[req->GetString("ssid")].id;
        strcat(cmd, std::to_string(id).c_str());
    } else {
        strcat(cmd, "connect ");
        strcat(cmd, req->GetString("ssid").c_str());
        strcat(cmd, " ");
        strcat(cmd, req->GetString("password").c_str());
    }

    printf("cmd: %s\n", cmd);
    fp = popen(cmd, "r");
    if (fp == NULL) {
        printf("Failed to run %s\n", cmd);
        return -1;
    }

    if (fgets(info, sizeof(info) - 1, fp) != NULL) {
        id = stoi(std::string(info));
    }

    if (fgets(info, sizeof(info) - 1, fp) != NULL) {
        msg = std::string(info);
        if (msg.back() == '\n') {
            msg.erase(msg.size() - 1);
        }
    }

    hv::Json response;

    if (msg.compare("OK") != 0) {
        response["code"] = 1112;
        response["msg"] = msg;
    } else {
        response["code"] = 0;
        response["msg"] = "";
        g_wifiInfo[req->GetString("ssid")] = {id, 1, 1};
    }
    response["data"] = hv::Json({});

    return resp->Json(response);
}

int disconnectWiFi(HttpRequest* req, HttpResponse* resp) {

    std::cout << "\ndisconnect WiFi...\n";
    std::cout << "ssid: " << req->GetString("ssid") << "\n";

    std::string msg;
    int id = g_wifiInfo[req->GetString("ssid")].id;
    FILE *fp;
    char info[128];
    char cmd[128] = "/mnt/wifitool.sh disconnect ";

    printf("id: %d\n", id);

    strcat(cmd, std::to_string(id).c_str());
    printf("cmd: %s\n", cmd);
    fp = popen(cmd, "r");
    if (fp == NULL) {
        printf("Failed to run %s\n", cmd);
        return -1;
    }

    if (fgets(info, sizeof(info) - 1, fp) != NULL) {
        msg = std::string(info);
        if (msg.back() == '\n') {
            msg.erase(msg.size() - 1);
        }
    }

    hv::Json response;

    if (msg.compare("OK") != 0) {
        response["code"] = 1112;
        response["msg"] = msg;
    } else {
        response["code"] = 0;
        response["msg"] = "";
    }
    response["data"] = hv::Json({});

    return resp->Json(response);
}

int switchWiFi(HttpRequest* req, HttpResponse* resp) {

    std::cout << "\nswitch WiFi operation...\n";
    std::cout << "mode: " << req->GetString("mode") << "\n";

    g_wifiMode = stoi(req->GetString("mode"));

    std::cout << "g_wifiMode: " << g_wifiMode << "\n";

    hv::Json response;
    response["code"] = 0;
    response["msg"] = "";
    response["data"] = hv::Json({});

    return resp->Json(response);
}

int getWifiStatus(HttpRequest* req, HttpResponse* resp) {

    hv::Json response;
    response["code"] = 0;
    response["msg"] = "";

    hv::Json data;
    data["status"] = 2;
    response["data"] = data;

    return resp->Json(response);
}

int autoConnectWiFi(HttpRequest* req, HttpResponse* resp) {

    std::cout << "\nauto Connect operation...\n";
    std::cout << "ssid: " << req->GetString("ssid") << "\n";
    std::cout << "mode: " << req->GetString("mode") << "\n";

    hv::Json response;
    response["code"] = 0;
    response["msg"] = "";
    response["data"] = hv::Json({});

    return resp->Json(response);
}

int forgetWiFi(HttpRequest* req, HttpResponse* resp) {

    std::cout << "\nforget WiFi operation...\n";
    std::cout << "ssid: " << req->GetString("ssid") << "\n";

    std::string msg;
    int id;
    FILE *fp;
    char info[128];
    char cmd[128] = "/mnt/wifitool.sh remove ";

    id = g_wifiInfo[req->GetString("ssid")].id;
    strcat(cmd, std::to_string(id).c_str());

    printf("cmd: %s\n", cmd);
    fp = popen(cmd, "r");
    if (fp == NULL) {
        printf("Failed to run %s\n", cmd);
        return -1;
    }

    if (fgets(info, sizeof(info) - 1, fp) != NULL) {
        msg = std::string(info);
        if (msg.back() == '\n') {
            msg.erase(msg.size() - 1);
        }
    }

    auto it = g_wifiInfo.find(req->GetString("ssid"));
    if (it != g_wifiInfo.end()) {
        g_wifiInfo.erase(it);
    }

    hv::Json response;
    response["code"] = 0;
    response["msg"] = "";
    response["data"] = hv::Json({});

    return resp->Json(response);
}
