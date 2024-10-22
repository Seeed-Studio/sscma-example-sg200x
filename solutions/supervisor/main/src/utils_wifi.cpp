#include <iostream>
#include <map>
#include <stdio.h>
#include <string.h>
#include <vector>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <algorithm>
#include <syslog.h>
#include <thread>

#include "hv/HttpServer.h"
#include "global_cfg.h"
#include "utils_wifi.h"

typedef struct _WIFI_INFO_S {
    int id;
    int connectedStatus;
    int autoConnect;
} WIFI_INFO_S;

std::vector<std::vector<std::string>> g_wifiList;
std::map<std::string, WIFI_INFO_S> g_wifiInfo;
std::string g_currentWifi;
bool g_wifiStatus = true;
int g_wifiMode = 1;
int g_etherConnected = 0;

static int getWifiInfo(std::vector<std::string>& wifiStatus)
{
    FILE* fp;
    char info[128];
    int index = 0;

    fp = popen(SCRIPT_WIFI_STATUS, "r");
    if (fp == NULL) {
        syslog(LOG_ERR, "Failed to run `%s`(%s)\n", SCRIPT_WIFI_STATUS, strerror(errno));
        return -1;
    }

    while (fgets(info, sizeof(info) - 1, fp) != NULL) {
        wifiStatus[index] = std::string(info);
        if (wifiStatus[index].back() == '\n') {
            wifiStatus[index].erase(wifiStatus[index].size() - 1);
        }

        ++index;
    }

    pclose(fp);
    return 0;
}

static std::string getWifiConnectStatus() {
    FILE* fp;
    char cmd[128] = SCRIPT_WIFI_CONNECT_STATUS;
    char connectStatus[128] = "";
    int len = 0;

    fp = popen(cmd, "r");
    if (fp == NULL) {
        syslog(LOG_ERR, "Failed to run `%s`(%s)\n", cmd, strerror(errno));
        return std::string("Failed");
    }

    fgets(connectStatus, sizeof(connectStatus) - 1, fp);
    len = strlen(connectStatus);
    if (connectStatus[len - 1] == '\n') {
        connectStatus[len - 1] = '\0';
    }

    pclose(fp);
    return std::string(connectStatus);
}

static std::string getWifiId() {
    FILE* fp;
    char cmd[128] = SCRIPT_WIFI_GET_WIFI_ID;
    char info[128] = "";
    int len = 0;

    fp = popen(cmd, "r");
    if (fp == NULL) {
        syslog(LOG_ERR, "Failed to run `%s`(%s)\n", cmd, strerror(errno));
        return std::string("-1");
    }

    fgets(info, sizeof(info) - 1, fp);
    len = strlen(info);
    if (info[len - 1] == '\n') {
        info[len - 1] = '\0';
    }
    pclose(fp);

    return std::string(info);
}

std::string getWifiIp() {
    FILE* fp;
    char cmd[128] = SCRIPT_WIFI_GET_WIFI_IP;
    char info[128] = "";
    int len = 0;

    fp = popen(cmd, "r");
    if (fp == NULL) {
        syslog(LOG_ERR, "Failed to run `%s`(%s)\n", cmd, strerror(errno));
        return std::string("");
    }

    fgets(info, sizeof(info) - 1, fp);
    len = strlen(info);
    if (info[len - 1] == '\n') {
        info[len - 1] = '\0';
    }
    pclose(fp);

    return std::string(info);
}

static int selectWifi(std::string id) {
    char cmd[128] = SCRIPT_WIFI_SELECT;

    strcat(cmd, id.c_str());
    system(cmd);

    return 0;
}

static std::string removeWifi(std::string id) {
    FILE* fp;
    char info[128] = "";
    char cmd[128] = SCRIPT_WIFI_REMOVE;
    int len = 0;

    strcat(cmd, id.c_str());
    fp = popen(cmd, "r");
    if (fp == NULL) {
        syslog(LOG_ERR, "Failed to run `%s`(%s)\n", cmd, strerror(errno));
        return std::string(info);
    }

    if (fgets(info, sizeof(info) - 1, fp) != NULL) {
        len = strlen(info);
        if (info[len - 1] == '\n') {
            info[len - 1] = '\0';
        }
    }

    pclose(fp);
    return std::string(info);
}

static void startAp() {
    system(SCRIPT_WIFI_START_AP);
}

static void stopAp() {
    system(SCRIPT_WIFI_STOP_AP);
}

static bool cmp(std::vector<std::string> lhs, std::vector<std::string> rhs) {
    if (lhs[0] == rhs[0]) {
        return std::stoi(lhs[1]) > std::stoi(rhs[1]);
    }
    return lhs[0] < rhs[0];
}

static int deduplicate(std::vector<std::vector<std::string>> &wifiList) {
    int size = wifiList.size();
    int l = 0, r = 1;

    while (r < size) {
        if (wifiList[l][0] == wifiList[r][0]) {
            r++;
        } else {
            wifiList[++l] = wifiList[r++];
        }
    }

    wifiList.erase(wifiList.begin() + l + 1, wifiList.end());

    return l;
}

static int getWifiList()
{
    FILE* fp;
    char info[128];
    char cmd[128] = SCRIPT_WIFI_SCAN_RESULTS;
    std::vector<std::vector<std::string>> wifiList;

    fp = popen(cmd, "r");
    if (fp == NULL) {
        syslog(LOG_ERR, "Failed to run `%s`(%s)\n", cmd, strerror(errno));
        return -1;
    }

    while (fgets(info, sizeof(info) - 1, fp) != NULL) {
        std::vector<std::string> wifi;

        char* token = strtok(info, " ");
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
        sort(wifiList.begin(), wifiList.end(), cmp);
        deduplicate(wifiList);

        g_wifiList = wifiList;
    }

    pclose(fp);
    return 0;
}

static int updateConnectedWifiInfo()
{
    FILE* fp;
    char info[128];

    syslog(LOG_INFO, "updateConnectedWifiInfo operation...\n");

    g_wifiInfo.clear();

    fp = popen(SCRIPT_WIFI_LIST, "r");
    if (fp == NULL) {
        syslog(LOG_ERR, "Failed to run `%s`(%s)\n", SCRIPT_WIFI_LIST, strerror(errno));
        return -1;
    }

    while (fgets(info, sizeof(info) - 1, fp) != NULL) {
        std::vector<std::string> wifi;

        char* token = strtok(info, " ");
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

static int getLocalNetInfo(const char *name, std::string &ip, std::string &mask, std::string &mac)
{
    int sock;
    struct ifreq ifr;
    char info[INET_ADDRSTRLEN];
    char mac_address[18];

    ip.clear();
    mask.clear();
    mac.clear();

    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        syslog(LOG_ERR, "[%s] socket failed here\n", name);
        return 1;
    }

    strncpy(ifr.ifr_name, name, IFNAMSIZ);

    if (ioctl(sock, SIOCGIFADDR, &ifr) < 0) {
        syslog(LOG_ERR, "[%s] ioctl SIOCGIFADDR failed here\n", name);
        close(sock);
        return 1;
    }
    inet_ntop(AF_INET, &((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr, info, INET_ADDRSTRLEN);
    ip = info;

    if (ioctl(sock, SIOCGIFNETMASK, &ifr) < 0) {
        syslog(LOG_ERR, "[%s] ioctl SIOCGIFNETMASK failed here\n", name);
        close(sock);
        return 1;
    }
    inet_ntop(AF_INET, &((struct sockaddr_in *)&ifr.ifr_netmask)->sin_addr, info, INET_ADDRSTRLEN);
    mask = info;

    if (ioctl(sock, SIOCGIFHWADDR, &ifr) < 0) {
        syslog(LOG_ERR, "[%s] ioctl SIOCGIFHWADDR failed here\n", name);
        close(sock);
        exit(1);
    }
    sprintf(mac_address, "%02x:%02x:%02x:%02x:%02x:%02x",
            (unsigned char)ifr.ifr_hwaddr.sa_data[0],
            (unsigned char)ifr.ifr_hwaddr.sa_data[1],
            (unsigned char)ifr.ifr_hwaddr.sa_data[2],
            (unsigned char)ifr.ifr_hwaddr.sa_data[3],
            (unsigned char)ifr.ifr_hwaddr.sa_data[4],
            (unsigned char)ifr.ifr_hwaddr.sa_data[5]);
    mac = mac_address;

    close(sock);

    syslog(LOG_INFO, "ip: %s\n", ip.c_str());
    syslog(LOG_INFO, "mask: %s\n", mask.c_str());
    syslog(LOG_INFO, "mac: %s\n", mac.c_str());

    return 0;
}

std::string getWiFiName(const char* ifrName) {
    std::string wifiName = "reCamera_xxxxxx";
    int fd;

    struct ifreq ifr;

    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        syslog(LOG_ERR, "socket");
        return wifiName;
    }

    strncpy(ifr.ifr_name, ifrName, IFNAMSIZ);

    if (ioctl(fd, SIOCGIFHWADDR, &ifr) < 0) {
        syslog(LOG_ERR, "ioctl");
        close(fd);
        return wifiName;
    }

    const char* data = ifr.ifr_hwaddr.sa_data;

    sprintf(&wifiName[0], "reCamera_%02x%02x%02x", (uint8_t) data[3], (uint8_t) data[4], (uint8_t) data[5]);

    syslog(LOG_INFO, "wifiName: %s\n", wifiName.c_str());

    close(fd);

    return wifiName;
}

void monitorWifiStatusThread() {
    std::string wifiStatus;
    bool apStatus = true;
    unsigned int cnt = 0;

    while (g_wifiStatus) {
        std::this_thread::sleep_for(std::chrono::seconds(10));
        wifiStatus = getWifiConnectStatus();

        if (wifiStatus == "COMPLETED" && !getWifiIp().empty()) {
            if (apStatus) {
                if (++cnt >= 12) {
                    apStatus = false;
                    stopAp();
                }
            }

            continue;
        }

        cnt = 0;

        if (wifiStatus == "DISCONNECTED" || wifiStatus == "INACTIVE" || wifiStatus == "Failed") {
            if (!apStatus) {
                apStatus = true;
                startAp();
            }
        }
    }
}

int queryWiFiInfo(HttpRequest* req, HttpResponse* resp)
{
    std::vector<std::string> wifiStatus(4, "");

    if (getWifiInfo(wifiStatus) != 0) {
        return -1;
    }

    hv::Json response;
    response["code"] = 0;
    response["msg"] = "";

    hv::Json data;
    data["status"] = g_wifiMode;

    hv::Json wifiInfo;

    wifiInfo["ssid"] = wifiStatus[0];
    g_currentWifi = wifiStatus[0];
    if (wifiStatus[1] != "") {
        wifiInfo["auth"] = 1;
    } else {
        wifiInfo["auth"] = 0;
    }
    wifiInfo["signal"] = 0;
    wifiInfo["connectedStatus"] = wifiStatus[0] != "-";
    wifiInfo["macAddres"] = wifiStatus[2];
    wifiInfo["ip"] = wifiStatus[3];
    wifiInfo["ipAssignment"] = 0;
    wifiInfo["subnetMask"] = "255.255.255.0";
    wifiInfo["autoConnect"] = 0;

    data["wifiInfo"] = wifiInfo;
    response["data"] = data;

    return resp->Json(response);
}

int scanWiFi(HttpRequest* req, HttpResponse* resp)
{
    syslog(LOG_INFO, "scan WiFi operation...\n");
    hv::Json response;
    FILE* fp;
    char cmd[128] = SCRIPT_WIFI_SCAN;
    char info[128] = "";
    int len = 0;

    fp = popen(cmd, "r");
    if (fp == NULL) {
        syslog(LOG_ERR, "Failed to run `%s`(%s)\n", cmd, strerror(errno));
        response["code"] = -1;
        response["msg"] = "Failed to scan WiFi";
        response["data"] = hv::Json({});
        return resp->Json(response);
    }

    fgets(info, sizeof(info) - 1, fp);
    len = strlen(info);
    if (info[len - 1] == '\n') {
        info[len - 1] = '\0';
    }
    pclose(fp);

    if (strcmp(info, "OK") == 0) {
        response["code"] = 0;
        response["msg"] = "Scan wifi successfully";
    } else {
        response["code"] = -1;
        response["msg"] = "Scanning WiFi too frequently";
    }
    response["data"] = hv::Json({});

    return resp->Json(response);
}

int getWiFiScanResults(HttpRequest* req, HttpResponse* resp)
{
    syslog(LOG_INFO, "get WiFi scan results operation...\n");

    std::string ip, mask, mac;
    hv::Json response;

    if (updateConnectedWifiInfo() != 0) {
        response["code"] = -1;
        response["msg"] = "Failed to update connected wifi information";
        response["data"] = hv::Json({});
        return resp->Json(response);
    }

    if (getWifiList() != 0) {
        response["code"] = -1;
        response["msg"] = "Failed to get wifi list";
        response["data"] = hv::Json({});
        return resp->Json(response);
    }

    response["code"] = 0;
    response["msg"] = "";

    hv::Json data;
    hv::Json etherInfo;
    hv::Json wifiInfo;
    std::vector<hv::Json> wifiInfoList;

    syslog(LOG_DEBUG, "current wifi: =%s=\n", g_currentWifi.c_str());

    if (getLocalNetInfo("wlan0", ip, mask, mac) == 0) {
        syslog(LOG_INFO, "wlan0 connect here\n");
    }

    for (auto wifi : g_wifiList) {
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

        if (wifi[0].compare(g_currentWifi) == 0) {
            wifiInfo["ip"] = ip;
            wifiInfo["ipAssignment"] = 0;
            wifiInfo["subnetMask"] = mask;
            wifiInfo["dns1"] = "-";
            wifiInfo["dns2"] = "-";
            wifiInfo["autoConnect"] = 0;
        } else {
            wifiInfo["ip"] = "";
            wifiInfo["ipAssignment"] = 0;
            wifiInfo["subnetMask"] = "-";
            wifiInfo["dns1"] = "-";
            wifiInfo["dns2"] = "-";
            wifiInfo["autoConnect"] = 0;
        }
        wifiInfoList.push_back(wifiInfo);
    }

    if (getLocalNetInfo("eth0", ip, mask, mac) == 0) {
        syslog(LOG_INFO, "eth0 connect here\n");
    }

    if (ip.empty() || ip.find("169.254") != std::string::npos) {
        g_etherConnected = 0;
        etherInfo["connectedStatus"] = 0;
        etherInfo["macAddres"] = "-";
        etherInfo["ip"] = "-";
        etherInfo["ipAssignment"] = 0;
        etherInfo["subnetMask"] = "-";
        etherInfo["dns1"] = "-";
        etherInfo["dns2"] = "-";
    } else {
        g_etherConnected = 1;
        etherInfo["connectedStatus"] = 1;
        etherInfo["macAddres"] = mac;
        etherInfo["ip"] = ip;
        etherInfo["ipAssignment"] = 0;
        etherInfo["subnetMask"] = mask;
        etherInfo["dns1"] = "-";
        etherInfo["dns2"] = "-";
    }

    data["etherinfo"] = etherInfo;
    data["wifiInfoList"] = hv::Json(wifiInfoList);
    response["data"] = data;

    // std::cout << "response: " << response.dump(2) << "\n";

    return resp->Json(response);
}

int connectWiFi(HttpRequest* req, HttpResponse* resp)
{
    syslog(LOG_INFO, "connect WiFi...\n");
    syslog(LOG_INFO, "ssid: %s\n", req->GetString("ssid").c_str());
    syslog(LOG_INFO, "password: %s\n", req->GetString("password").c_str());

    hv::Json response;
    std::string msg, currentWifiId;
    int id = 0, connecting = 0;
    FILE* fp;
    char info[128];
    char cmd[128] = "";

    currentWifiId = getWifiId();
    if (req->GetString("password").empty()) {
        strcpy(cmd, SCRIPT_WIFI_SELECT);
        updateConnectedWifiInfo();
        id = g_wifiInfo[req->GetString("ssid")].id;
        strcat(cmd, std::to_string(id).c_str());
    } else {
        strcpy(cmd, SCRIPT_WIFI_CONNECT);
        strcat(cmd, req->GetString("ssid").c_str());
        strcat(cmd, " ");
        strcat(cmd, req->GetString("password").c_str());
    }

    syslog(LOG_DEBUG, "cmd: %s\n", cmd);
    fp = popen(cmd, "r");
    if (fp == NULL) {
        syslog(LOG_ERR, "Failed to run `%s`(%s)\n", cmd, strerror(errno));
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

    pclose(fp);

    if (msg.compare("OK") != 0) {
        response["code"] = -1;
        response["msg"] = msg;
        response["data"] = hv::Json({});
        return resp->Json(response);
    }

    while (true) {
        std::string connectStatus = getWifiConnectStatus();

        if (connectStatus == "ASSOCIATING") {
            connecting = 1;
        }

        if (!connecting) {
            continue;
        }

        if (connectStatus == "COMPLETED" && !getWifiIp().empty()) {
            response["code"] = 0;
            response["msg"] = "Connection successful";
            g_wifiInfo[req->GetString("ssid")] = { id, 1, 1 };
            break;
        }

        if (connectStatus == "DISCONNECTED" || connectStatus == "INACTIVE" || connectStatus == "Failed") {
            if (req->GetString("password").size()) {
                removeWifi(std::to_string(id));
            }
            if (currentWifiId != "-1") {
                selectWifi(currentWifiId);
            }
            response["code"] = -1;
            response["msg"] = "Connection failed";
            response["data"] = hv::Json({});
            break;
        }

        usleep(10 * 1000);
    }

    return resp->Json(response);
}

int disconnectWiFi(HttpRequest* req, HttpResponse* resp)
{
    syslog(LOG_INFO, "disconnect WiFi...\n");
    syslog(LOG_INFO, "ssid: %s\n", req->GetString("ssid").c_str());

    std::string msg;
    int id = 0;
    FILE* fp;
    char info[128];
    char cmd[128] = SCRIPT_WIFI_DISCONNECT;

    updateConnectedWifiInfo();
    id = g_wifiInfo[req->GetString("ssid")].id;
    strcat(cmd, std::to_string(id).c_str());

    syslog(LOG_DEBUG, "id: %d\n", id);
    syslog(LOG_DEBUG, "cmd: %s\n", cmd);

    fp = popen(cmd, "r");
    if (fp == NULL) {
        syslog(LOG_ERR, "Failed to run `%s`(%s)\n", cmd, strerror(errno));
        return -1;
    }

    if (fgets(info, sizeof(info) - 1, fp) != NULL) {
        msg = std::string(info);
        if (msg.back() == '\n') {
            msg.erase(msg.size() - 1);
        }
    }
    pclose(fp);

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

int switchWiFi(HttpRequest* req, HttpResponse* resp)
{
    syslog(LOG_INFO, "switch WiFi operation...\n");
    syslog(LOG_INFO, "mode: %s\n", req->GetString("mode").c_str());

    g_wifiMode = stoi(req->GetString("mode"));

    syslog(LOG_DEBUG, "g_wifiMode: %d\n", g_wifiMode);

    hv::Json response;
    response["code"] = 0;
    response["msg"] = "";
    response["data"] = hv::Json({});

    return resp->Json(response);
}

int getWifiStatus(HttpRequest* req, HttpResponse* resp)
{
    FILE* fp;
    char info[128];
    hv::Json response, data;

    response["code"] = 0;
    response["msg"] = "";

    fp = popen(SCRIPT_WIFI_STATE, "r");
    if (fp == NULL) {
        syslog(LOG_ERR, "Failed to run `%s`(%s)\n", SCRIPT_WIFI_STATE, strerror(errno));
        return -1;
    }

    if (fgets(info, sizeof(info) - 1, fp) != NULL) {
        std::string s(info);
        if (s.back() == '\n') {
            s.erase(s.size() - 1);
        }

        if (s.compare("COMPLETED") == 0) {
            g_wifiMode = 3;     // wifi connected
            data["status"] = 1; // wifi connected
        } else if (s.compare("INACTIVE") == 0) {
            g_wifiMode = 1;     // wifi is on
            data["status"] = 2; // wifi not connected
        } else {
            g_wifiMode = 2;     // wifi connecting
            data["status"] = 2; // wifi not connected
        }
    }

    if (g_etherConnected) {
        data["status"] = 0;     // ethernet connected
    }

    pclose(fp);
    response["data"] = data;

    return resp->Json(response);
}

int autoConnectWiFi(HttpRequest* req, HttpResponse* resp)
{
    syslog(LOG_INFO, "auto Connect operation...\n");
    syslog(LOG_INFO, "ssid: %s\n", req->GetString("ssid").c_str());
    syslog(LOG_INFO, "mode: %s\n", req->GetString("mode").c_str());

    hv::Json response;
    response["code"] = 0;
    response["msg"] = "";
    response["data"] = hv::Json({});

    return resp->Json(response);
}

int forgetWiFi(HttpRequest* req, HttpResponse* resp)
{
    syslog(LOG_INFO, "forget WiFi operation...\n");
    syslog(LOG_INFO, "ssid: %s\n", req->GetString("ssid").c_str());

    std::string msg;
    int id;

    updateConnectedWifiInfo();
    id = g_wifiInfo[req->GetString("ssid")].id;
    removeWifi(std::to_string(id));

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
