#include <fstream>
#include <iostream>
#include <iterator>
#include <stdio.h>
#include <string>
#include <syslog.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "hv/HttpServer.h"
#include "global_cfg.h"
#include "utils_device.h"
#include "daemon.h"

SERVICE_STATUS systemStatus = SERVICE_STATUS_STARTING;
int g_progress = 0;
int g_restart = 0;

SERVICE_STATUS convertStatus(APP_STATUS appStatus) {
    switch (appStatus) {
        case APP_STATUS_NORMAL:
            return SERVICE_STATUS_NORMAL;

        case APP_STATUS_STOP:
            return SERVICE_STATUS_ERROR;

        case APP_STATUS_NORESPONSE:
            return SERVICE_STATUS_ERROR;
    }

    return SERVICE_STATUS_ERROR;
}

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
        syslog(LOG_DEBUG, "Write Success: %s\n", path.c_str());
        return 0;
    }

    return -1;
}

int createFolder(const char* dirName) {
    struct stat dirStat;
    mode_t mode = 0755;

    if (stat(dirName, &dirStat) != 0) {
        if (mkdir(dirName, mode) != 0) {
            syslog(LOG_DEBUG, "Failed to create %s folder\n", dirName);
            return -1;
        }
    }

    return 0;
}

static int saveModelFile(const HttpContextPtr& ctx, std::string filePath) {
    const char* modelKey = "model_file";
    auto form = ctx->request->GetForm();
    HFile file;

    if (form.empty()) {
        return HTTP_STATUS_BAD_REQUEST;
    }

    auto iter = form.find(modelKey);
    if (iter == form.end()) {
        return HTTP_STATUS_BAD_REQUEST;
    }

    const auto& formdata = iter->second;
    if (formdata.content.empty()) {
        return HTTP_STATUS_BAD_REQUEST;
    }

    filePath += "model.cvimodel";
    if (file.open(filePath.c_str(), "wb") != 0) {
        return HTTP_STATUS_INTERNAL_SERVER_ERROR;
    }
    file.write(formdata.content.data(), formdata.content.size());

    return 200;
}

static int saveModelInfoFile(const HttpContextPtr& ctx, std::string filePath) {
    const char* modelKey = "model_info";
    std::string modelInfo = ctx->request->GetFormData(modelKey);
    HFile file;

    filePath += "model.json";
    if (file.open(filePath.c_str(), "wb") != 0) {
        syslog(LOG_DEBUG, "open %s file failed\n", filePath.c_str());
        return HTTP_STATUS_INTERNAL_SERVER_ERROR;
    } else {
        file.write(modelInfo.c_str(), modelInfo.size());
    }

    return 200;
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
        syslog(LOG_ERR, "Failed to run `%s`\n", cmd);
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

static void clearNewline(char* value, int len) {
    if (value[len - 1] == '\n') {
        value[len - 1] = '\0';
    }
}

static void clearNewline(std::string& value) {
    if (value.back() == '\n') {
        value.erase(value.size() - 1);
    }
}

void initSystemStatus() {
    FILE* fp;
    char cmd[128] = SCRIPT_DEVICE_GETSYSTEMSTATUS;
    char info[128] = "";

    fp = popen(cmd, "r");
    if (fp == NULL) {
        syslog(LOG_ERR, "Failed to run %s\n", cmd);
        systemStatus = SERVICE_STATUS_ERROR;
        return ;
    }

    fgets(info, sizeof(info) - 1, fp);
    clearNewline(info, strlen(info));

    if (strlen(info) == 0) {
        systemStatus = SERVICE_STATUS_NORMAL;
    } else {
        systemStatus = SERVICE_STATUS_ERROR;
    }

    pclose(fp);
}

int getSystemStatus(HttpRequest* req, HttpResponse* resp) {
    FILE* fp;
    char cmd[128] = SCRIPT_DEVICE_GETSYSTEMSTATUS;
    char info[128] = "";
    hv::Json response;

    fp = popen(cmd, "r");
    if (fp == NULL) {
        syslog(LOG_ERR, "Failed to run %s\n", cmd);
        response["code"] = -1;
        response["msg"] = "Failed to run " + std::string(cmd);
        response["data"] = hv::Json({});
        return resp->Json(response);
    }

    fgets(info, sizeof(info) - 1, fp);
    clearNewline(info, strlen(info));

    if (strlen(info) == 0) {
        response["code"] = 0;
        response["msg"] = "";
    } else {
        response["code"] = -1;
        response["msg"] = "System damage";
    }

    pclose(fp);

    response["data"] = hv::Json({});

    return resp->Json(response);
}

int queryServiceStatus(HttpRequest* req, HttpResponse* resp) {
    hv::Json response;

    response["code"] = 0;
    response["msg"] = "";
    response["data"]["sscma-node"] = convertStatus(sscmaStatus);
    response["data"]["node-node"] = noderedStarting ? SERVICE_STATUS_STARTING : convertStatus(noderedStatus);
    response["data"]["upgrade"] = systemStatus;

    return resp->Json(response);
}

int getSystemUpdateVesionInfo(HttpRequest* req, HttpResponse* resp)
{
    syslog(LOG_INFO, "start to get SystemUpdateVersinInfo...\n");

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

    syslog(LOG_DEBUG, "cmd: %s\n", cmd.c_str());
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

    syslog(LOG_INFO, "content: %s\n", content.c_str());
    syslog(LOG_INFO, "progress: %d\n, msg: %s\n", progress, msg.c_str());
    syslog(LOG_INFO, "os: %s, version: %s\n", os.c_str(), version.c_str());

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
    data["appName"] = "supervisor";
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
    syslog(LOG_INFO, "Device name: %s\n", data["deviceName"].template get<std::string>().c_str());
    syslog(LOG_INFO, "OS,Version: %s\n", os_version.c_str());
    syslog(LOG_INFO, "Channel,Url: %s\n", ch_url.c_str());

    return resp->Json(device);
}

int updateDeviceName(HttpRequest* req, HttpResponse* resp)
{
    std::string dev_name = req->GetString("deviceName");

    syslog(LOG_INFO, "update Device Name operation...\n");
    syslog(LOG_INFO, "deviceName: %s\n", dev_name.c_str());

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

    syslog(LOG_INFO, "update channel operation...\n");
    syslog(LOG_INFO, "channel: %s\n", str_ch.c_str());
    syslog(LOG_INFO, "serverUrl: %s\n", str_url.c_str());

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
    syslog(LOG_INFO, "set Power operation...\n");
    syslog(LOG_INFO, "mode: %s\n", req->GetString("mode").c_str());

    int mode = stoi(req->GetString("mode"));

    if (mode) {
        syslog(LOG_INFO, "start to reboot system\n");
        system("reboot");
    } else {
        syslog(LOG_INFO, "start to shut down system\n");
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
    syslog(LOG_INFO, "start to update System now...\n");

    char cmd[128] = SCRIPT_UPGRADE_START;

    strcat(cmd, "&");
    system(cmd);

    hv::Json response;

    response["code"] = 0;
    response["msg"] = "";
    response["data"] = hv::Json({});

    return resp->Json(response);
}

int getUpdateProgress(HttpRequest* req, HttpResponse* resp)
{
    syslog(LOG_INFO, "get Update Progress...\n");

    FILE* fp;
    char info[128];
    hv::Json response, data;

    fp = popen(SCRIPT_UPGRADE_QUERY, "r");
    if (fp == NULL) {
        syslog(LOG_ERR, "Failed to run `%s`\n", SCRIPT_UPGRADE_QUERY);
        return -1;
    }

    while (fgets(info, sizeof(info) - 1, fp) != NULL) {
        std::string s(info);
        if (s.back() == '\n') {
            s.erase(s.size() - 1);
        }

        syslog(LOG_INFO, "info: %s\n", s.c_str());
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
    syslog(LOG_INFO, "cancel update...\n");

    system(SCRIPT_UPGRADE_STOP);

    hv::Json response;

    response["code"] = 0;
    response["msg"] = "";
    response["data"] = hv::Json({});

    return resp->Json(response);
}

int getDeviceList(HttpRequest* req, HttpResponse* resp) {
    FILE* fp;
    char cmd[128] = SCRIPT_DEVICE_GETADDRESSS;
    char info[128] = "";
    std::string deviceName = readFile(PATH_DEVICE_NAME);
    hv::Json response, data;

    strcat(cmd, req->client_addr.ip.c_str());
    fp = popen(cmd, "r");
    if (fp == NULL) {
        syslog(LOG_ERR, "Failed to run `%s`\n", cmd);
        return -1;
    }

    fgets(info, sizeof(info) - 1, fp);
    clearNewline(info, strlen(info));

    data.push_back({
        {"deviceName", deviceName},
        {"ip", info}
    });

    if (strlen(info) == 0) {
        response["code"] = 1;
        response["mgs"] = "";
    } else {
        response["code"] = 0;
        response["mgs"] = "";
    }
    response["data"]["deviceList"] = data;

    pclose(fp);

    return resp->Json(response);
}

int getDeviceInfo(HttpRequest* req, HttpResponse* resp) {
    std::string os_version = readFile(PATH_ISSUE);
    std::string os = "Null", version = "Null";
    size_t pos;

    clearNewline(os_version);
    pos = os_version.find(' ');
    if (pos != std::string::npos) {
        os = os_version.substr(0, pos);
        version = os_version.substr(pos + 1);
    }

    FILE* fp;
    char cmd[128] = SCRIPT_DEVICE_GETADDRESSS;
    char info[128] = "";

    strcat(cmd, req->client_addr.ip.c_str());
    fp = popen(cmd, "r");
    if (fp == NULL) {
        syslog(LOG_ERR, "Failed to run `%s`\n", cmd);
        return -1;
    }

    fgets(info, sizeof(info) - 1, fp);
    clearNewline(info, strlen(info));

    hv::Json response, data;

    data["deviceName"] = os;
    data["ip"] = info;
    data["status"] = 1;
    data["osVersion"] = version;
    data["sn"] = "-";

    response["code"] = 0;
    response["mgs"] = "";
    response["data"] = data;

    pclose(fp);

    return resp->Json(response);
}

int getAppInfo(HttpRequest* req, HttpResponse* resp) {
    FILE* fp;
    char cmd[128] = SCRIPT_DEVICE_GETAPPINFO;
    char appName[20] = "", appVersion[10] = "";

    fp = popen(cmd, "r");
    if (fp == NULL) {
        syslog(LOG_ERR, "Failed to run %s\n", cmd);
        return -1;
    }

    fgets(appName, sizeof(appName) - 1, fp);
    clearNewline(appName, strlen(appName));

    fgets(appVersion, sizeof(appVersion) - 1, fp);
    clearNewline(appVersion, strlen(appVersion));

    hv::Json response;
    response["code"] = 0;
    response["msg"] = "";
    response["data"]["appName"] = appName;
    response["data"]["appVersion"] = appVersion;

    pclose(fp);

    return resp->Json(response);
}

int uploadApp(const HttpContextPtr& ctx) {
    int ret = 0;
    std::string appPath = PATH_APP_DOWNLOAD_DIR;
    FILE* fp;
    char cmd[128] = SCRIPT_DEVICE_INSTALLAPP;
    char info[128] = "";

    if (ctx->param("filename").empty()) {
        syslog(LOG_ERR, "Missing filename parameter value\n");
    }

    if (ctx->is(MULTIPART_FORM_DATA)) {
        ret = ctx->request->SaveFormFile("file", appPath.c_str());
    } else {
        std::string fileName = ctx->param("filename", "app.zip");
        std::string filePath = appPath + fileName;
        ret = ctx->request->SaveFile(filePath.c_str());
    }

    strcat(cmd, std::string(appPath + ctx->param("filename")).c_str());
    strcat(cmd, " ");
    strcat(cmd, ctx->param("appName").c_str());
    strcat(cmd, " ");
    strcat(cmd, ctx->param("appVersion").c_str());
    fp = popen(cmd, "r");
    if (fp == NULL) {
        syslog(LOG_ERR, "Failed to run %s\n", cmd);
        return -1;
    }

    fgets(info, sizeof(info) - 1, fp);
    clearNewline(info, strlen(info));

    syslog(LOG_INFO, "info: %s\n", info);

    hv::Json response;
    if (strcmp(info, "Finished") == 0) {
        response["code"] = 0;
        response["msg"] = "";
    } else {
        response["code"] = -1;
        response["msg"] = info;
    }
    response["data"] = hv::Json({});

    pclose(fp);

    ctx->response.get()->Set("code", 200);
    ctx->response.get()->Set("message", response.dump(2));
    return 200;
}

int getModelInfo(HttpRequest* req, HttpResponse* resp) {
    std::string modelInfo;
    std::string filePath = PATH_MODEL_DOWNLOAD_DIR + std::string("model.json");
    hv::Json response;

    modelInfo = readFile(filePath, "NULL");

    if (modelInfo == "NULL") {
        response["code"] = -1;
        response["msg"] = "File does not exist";
        response["data"] = hv::Json({});
    } else {
        response["code"] = 0;
        response["msg"] = "";
        response["data"]["model_info"] = modelInfo;
    }

    return resp->Json(response);
}

int uploadModel(const HttpContextPtr& ctx) {
    int ret = 0;
    std::string modelPath = PATH_MODEL_DOWNLOAD_DIR;

    if (ctx->is(MULTIPART_FORM_DATA)) {
        ret = createFolder(PATH_MODEL_DOWNLOAD_DIR);
        if (ret == 0) {
            ret = saveModelFile(ctx, modelPath);
        }
        if (ret == 200) {
            ret = saveModelInfoFile(ctx, modelPath);
        }
    } else {
        std::string fileName = ctx->param("model_file", "model.cvimodel");
        std::string filePath = modelPath + fileName;
        ret = ctx->request->SaveFile(filePath.c_str());
    }

    hv::Json response;
    if (ret == 200) {
        response["code"] = 0;
        response["msg"] = "upload model successfully";
    } else {
        response["code"] = -1;
        response["msg"] = "upload model failed";
    }
    response["data"] = hv::Json({});

    return ctx->send(response.dump(2));
}
