#include <fstream>
#include <iostream>
#include <iterator>
#include <stdio.h>
#include <string>

#include "hv/HttpServer.h"
#include "global_cfg.h"
#include "utils_device.h"

int g_progress = 0;
int g_restart = 0;

std::string readFile(const std::string& path, const std::string& defaultname)
{
    std::ifstream ifs(path);
    if (!ifs.is_open()) {
        // 文件无法打开，返回默认值
        return defaultname;
    }
    // 读取文件内容并返回
    return std::string((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));
}

static int writeFile(const std::string& path, const std::string& strWrite)
{
    std::ofstream outfile(path);
    if (outfile.is_open()) {
        outfile << strWrite;
        outfile.close();
        std::cout << "Write Success: " << path << std::endl;
        return 0;
    }

    return -1;
}

static std::string getGateWay(std::string ip)
{
    FILE* fp;
    char info[128];
    char cmd[128] = SCRIPT_WIFI_GATEWAY;
    std::string res;

    strcat(cmd, ip.c_str());
    fp = popen(cmd, "r");
    if (fp == NULL) {
        printf("Failed to run `%s`\n", cmd);
        return res;
    }

    if (fgets(info, sizeof(info) - 1, fp) != NULL) {
        res = std::string(info);
        if (res.back() == '\n') {
            res.erase(res.size() - 1);
        }
    }
    pclose(fp);

    return res;
}

int getSystemUpdateVesionInfo(HttpRequest* req, HttpResponse* resp)
{
    std::cout << "\nstart to get SystemUpdateVersinInfo...\n";

    std::string content = readFile(PATH_UPGRADE_URL), url = "";
    std::string cmd = SCRIPT_UPGRADE_LATEST, msg = "";
    std::string os = "Null", version = "Null";
    int channel = 0, progress = 0;
    size_t pos = content.find(',');
    hv::Json response, data;

    if (pos != std::string::npos) {
        channel = stoi(content.substr(0, pos));
        url = content.substr(pos + 1);
        if (url.back() == '\n') {
            url.erase(url.size() - 1);
        }
    }

    if (channel == 0) {
        cmd += DEFAULT_UPGRADE_URL;
    } else {
        cmd += url;
    }

    std::cout << "cmd: " << cmd << "\n";
    system(cmd.c_str());

    content = readFile(PATH_UPGRADE_PROGRESS_FILE);
    pos = content.find(',');
    if (pos != std::string::npos) {
        progress = stoi(content.substr(0, pos));
        msg = content.substr(pos + 1);
        if (msg.back() == '\n') {
            msg.erase(msg.size() - 1);
        }
    }

    content = readFile(PATH_UPGRADE_VERSION_FILE);
    pos = content.find(' ');
    if (pos != std::string::npos) {
        os = content.substr(0, pos);
        version = content.substr(pos + 1);
    }
    if (version.back() == '\n') {
        version.erase(version.size() - 1);
    }

    std::cout << "content: " << content << "\n";
    std::cout << "progress: " << progress << ", msg: " << msg << "\n";
    std::cout << "os: " << os << ", version: " << version << "\n";

    if (progress == 0) {
        response["code"] = 0;
        response["msg"] = msg;
    } else if (progress >= 11) {
        response["code"] = 0;
        response["msg"] = "";
    } else if (progress == 3 || progress == 4) {
        system(cmd.c_str());
        response["code"] = 1103;
        response["msg"] = msg;
    } else {
        response["code"] = 1106;
        response["msg"] = msg;
    }

    data["osName"] = os;
    data["osVersion"] = version;
    data["downloadUrl"] = "";
    response["data"] = data;

    return resp->Json(response);
}

int queryDeviceInfo(HttpRequest* req, HttpResponse* resp)
{
    hv::Json device;
    device["code"] = 0;
    device["msg"] = "";

    std::string os_version = readFile(PATH_ISSUE);
    std::string os = "Null", version = "Null";
    size_t pos = os_version.find(' ');
    if (pos != std::string::npos) {
        os = os_version.substr(0, pos);
        version = os_version.substr(pos + 1);
    }
    if (version.back() == '\n') {
        version.erase(version.size() - 1);
    }

    std::string ch_url = readFile(PATH_UPGRADE_URL);
    std::string ch = "0", url = "";
    pos = ch_url.find(',');
    if (pos != std::string::npos) {
        ch = ch_url.substr(0, pos);
        url = ch_url.substr(pos + 1);
    }
    if (url.back() == '\n') {
        url.erase(url.size() - 1);
    }

    hv::Json data;
    data["deviceName"] = readFile(PATH_DEVICE_NAME);
    data["ip"] = req->host;
    data["mask"] = "255.255.255.0";
    data["gateway"] = getGateWay(req->host);
    data["dns"] = "-";
    data["channel"] = std::stoi(ch);
    data["serverUrl"] = url;
    data["officialUrl"] = DEFAULT_UPGRADE_URL;
    data["cpu"] = "sg2002";
    data["ram"] = 256;
    data["npu"] = 1;
    data["osName"] = os;
    data["osVersion"] = version;
    data["osUpdateTime"] = "2024.01.01";
    data["terminalPort"] = TTYD_PORT;
    data["needRestart"] = g_restart;

    device["data"] = data;

    // output
    std::cout << "Device name: " << data["deviceName"] << std::endl;
    std::cout << "OS,Version: " << os_version << std::endl;
    std::cout << "Channel,Url: " << ch_url << std::endl;

    return resp->Json(device);
}

int updateDeviceName(HttpRequest* req, HttpResponse* resp)
{
    std::string dev_name = req->GetString("deviceName");

    std::cout << "\nupdate Device Name operation...\n";
    std::cout << "deviceName: " << dev_name << "\n";
    writeFile(PATH_DEVICE_NAME, dev_name);

    hv::Json response;
    response["code"] = 0;
    response["msg"] = "";
    response["data"] = hv::Json({});

    return resp->Json(response);
}

int updateChannel(HttpRequest* req, HttpResponse* resp)
{
    hv::Json response;
    std::string str_ch = req->GetString("channel");
    std::string str_url = req->GetString("serverUrl");
    std::string str_cmd;

    std::cout << "\nupdate channel operation...\n";
    std::cout << "channel: " << str_ch << "\n";
    std::cout << "serverUrl: " << str_url << "\n";

    if (str_ch.empty()) {
        response["code"] = 1109;
        response["msg"] = "value error";
        response["data"] = hv::Json({});

        return resp->Json(response);
    }

    if (str_ch.compare("0") == 0) {
        str_cmd = "sed -i \"s/1,/0,/g\" ";
        str_cmd += PATH_UPGRADE_URL;
    } else {
        str_cmd = "echo " + str_ch + "," + str_url + " > ";
        str_cmd += PATH_UPGRADE_URL;
    }

    system(str_cmd.c_str());

    response["code"] = 0;
    response["msg"] = "";
    response["data"] = hv::Json({});

    return resp->Json(response);
}

int setPower(HttpRequest* req, HttpResponse* resp)
{
    std::cout << "\nset Power operation...\n";
    std::cout << "mode: " << req->GetString("mode") << "\n";

    int mode = stoi(req->GetString("mode"));

    if (mode) {
        printf("start to reboot system\n");
        system("reboot");
    } else {
        printf("start to shut down system\n");
        system("poweroff");
    }

    hv::Json response;

    response["code"] = 0;
    response["msg"] = "";
    response["data"] = hv::Json({});

    return resp->Json(response);
}

/* upgrade */
int updateSystem(HttpRequest* req, HttpResponse* resp)
{
    std::cout << "\nstart to update System now...\n";

    std::string ch_url = readFile(PATH_UPGRADE_URL), url = "";
    std::string cmd = SCRIPT_UPGRADE_START;
    int channel = 0;
    size_t pos = ch_url.find(',');
    if (pos != std::string::npos) {
        channel = stoi(ch_url.substr(0, pos));
        url = ch_url.substr(pos + 1);
        if (url.back() == '\n') {
            url.erase(url.size() - 1);
        }
    }

    if (channel == 0) {
        cmd += " ";
        cmd += DEFAULT_UPGRADE_URL;
        cmd += " &";
    } else {
        cmd += " " + url + " &";
    }

    system(cmd.c_str());

    hv::Json response;

    response["code"] = 0;
    response["msg"] = "";
    response["data"] = hv::Json({});

    return resp->Json(response);
}

int getUpdateProgress(HttpRequest* req, HttpResponse* resp)
{
    std::cout << "\nget Update Progress...\n";

    FILE* fp;
    char info[128];
    hv::Json response, data;

    fp = popen(SCRIPT_UPGRADE_QUERY, "r");
    if (fp == NULL) {
        printf("Failed to run `%s`\n", SCRIPT_UPGRADE_QUERY);
        return -1;
    }

    while (fgets(info, sizeof(info) - 1, fp) != NULL) {
        std::string s(info);
        if (s.back() == '\n') {
            s.erase(s.size() - 1);
        }
        std::cout << "info: " << s << "\n";
        size_t pos = s.find(',');
        if (pos != std::string::npos) {
            g_progress = stoi(s.substr(0, pos));
            response["code"] = 1106;
            response["msg"] = s.substr(pos + 1, s.size() - 1);
        } else {
            g_progress = stoi(s);
            response["code"] = 0;
            response["msg"] = "";
        }
    }

    pclose(fp);

    if (g_progress == 100) {
        g_restart = 1;
    }

    data["progress"] = g_progress;
    response["data"] = data;

    return resp->Json(response);
}

int cancelUpdate(HttpRequest* req, HttpResponse* resp)
{
    std::cout << "\ncancel update...\n";

    system(SCRIPT_UPGRADE_STOP);

    hv::Json response;

    response["code"] = 0;
    response["msg"] = "";
    response["data"] = hv::Json({});

    return resp->Json(response);
}
