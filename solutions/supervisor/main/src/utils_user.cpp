#include <iostream>
#include <fstream>
#include <string>
#include <pwd.h>
#include <stdio.h>
#include <unistd.h>
#include <vector>
#include <syslog.h>

#include "hv/HttpServer.h"
#include "global_cfg.h"
#include "utils_user.h"
#include "utils_device.h"

static std::string g_sUserName;
static std::string g_sPassword;
int g_userId = 0;

static void clearNewline(char* value, int len) {
    if (value[len - 1] == '\n') {
        value[len - 1] = '\0';
    }
}

int initUserInfo() {
    FILE* fp;
    char info[128];
    char cmd[128] = SCRIPT_USER_ID;

    fp = popen(cmd, "r");
    if (fp == NULL) {
        syslog(LOG_ERR, "Failed to run `%s`(%s)\n", cmd, strerror(errno));
        return -1;
    }

    fgets(info, sizeof(info) - 1, fp);
    g_userId = std::stoi(info);

    pclose(fp);

    return 0;
}

static int getUserName()
{
    char username[128];
    struct passwd* pw = getpwuid(g_userId);

    if (pw) {
        strncpy(username, pw->pw_name, sizeof(username));
    } else {
        syslog(LOG_ERR, "getpwuid failed here\n");
        return -1;
    }

    g_sUserName.assign(username);
    return 0;
}

static int verifyPasswd(const std::string& passwd) {
    FILE* fp;
    char cmd[128] = SCRIPT_USER_VERIFY;
    char info[128] = "";

    strcat(cmd, passwd.c_str());
    fp = popen(cmd, "r");
    if (fp == NULL) {
        syslog(LOG_ERR, "Failed to run `%s`(%s)\n", cmd, strerror(errno));
        return -1;
    }

    fgets(info, sizeof(info) - 1, fp);
    clearNewline(info, strlen(info));
    pclose(fp);

    if (strcmp(info, "ERROR") == 0) {
        return -1;
    }

    return 0;
}

static void savePasswd(const std::string& passwd) {
    char cmd[128] = SCRIPT_USER_SAVE;

    strcat(cmd, passwd.c_str());
    system(cmd);
}

static int verifyKey(const std::string& keyValue) {
    FILE* fp;
    char cmd[128] = SCRIPT_USER_VERIFY_SSH;
    char result[128] = "";
    std::ofstream file;

    file.open(PATH_TMP_KEY_FILE);
    if (!file) {
        syslog(LOG_ERR, "Failed to open %s file(%s)\n", PATH_TMP_KEY_FILE, strerror(errno));
        return -1;
    }

    file << keyValue;
    file.close();

    strcat(cmd, PATH_TMP_KEY_FILE);
    fp = popen(cmd, "r");
    if (fp == NULL) {
        syslog(LOG_ERR, "Failed to run `%s`(%s)\n", cmd, strerror(errno));
        return -2;
    }

    fgets(result, sizeof(result) - 1, fp);
    clearNewline(result, strlen(result));
    pclose(fp);
    remove(PATH_TMP_KEY_FILE);

    if (strcmp(result, "OK") != 0) {
        return -1;
    }

    return 0;
}

int queryUserInfo(HttpRequest* req, HttpResponse* resp)
{
    hv::Json User;
    User["code"] = 0;
    User["msg"] = "";

    getUserName();
    syslog(LOG_DEBUG, "%s, %s\n", g_sUserName.c_str());

    hv::Json data;
    data["userName"] = g_sUserName;

    FILE* fp;
    char info[128];
    std::vector<hv::Json> sshkeyList;

    fp = popen(SCRIPT_USER_SSH, "r");
    if (fp == NULL) {
        syslog(LOG_ERR, "Failed to run `%s`(%s)\n", SCRIPT_USER_SSH, strerror(errno));
    }

    while (fgets(info, sizeof(info) - 1, fp) != NULL) {
        std::vector<std::string> keyInfo(4, "-");
        std::string s(info);
        hv::Json sshkey;
        int cnt = 0;

        if (s.back() == '\n') {
            s.erase(s.size() - 1);
        }

        size_t pos = s.find(' ');
        while (pos < std::string::npos) {
            keyInfo[cnt++] = s.substr(0, pos);
            s = s.substr(pos + 1);
            pos = s.find(' ');
        }

        sshkey["id"] = keyInfo[1];
        sshkey["name"] = keyInfo[2];
        sshkey["value"] = keyInfo[0];
        sshkey["addTime"] = keyInfo[3];
        sshkeyList.push_back(sshkey);
    }
    pclose(fp);

    data["sshkeyList"] = hv::Json(sshkeyList);
    User["data"] = data;

    return resp->Json(User);
}

int updateUserName(HttpRequest* req, HttpResponse* resp)
{
    syslog(LOG_INFO, "update UserName operation...\n");
    syslog(LOG_INFO, "userName: %s\n", req->GetString("userName").c_str());

    FILE* fp;
    char cmd[128] = SCRIPT_USER_NAME;

    strcat(cmd, g_sUserName.c_str());
    strcat(cmd, " ");
    strcat(cmd, req->GetString("userName").c_str());
    syslog(LOG_DEBUG, "cmd: %s\n", cmd);

    fp = popen(cmd, "r");
    if (fp == NULL) {
        syslog(LOG_ERR, "Failed to run `%s`(%s)\n", cmd, strerror(errno));
        return -1;
    }
    pclose(fp);

    hv::Json User;
    User["code"] = 0;
    User["msg"] = "";
    User["data"] = hv::Json({});

    return resp->Json(User);
}

int updatePassword(HttpRequest* req, HttpResponse* resp)
{
    syslog(LOG_INFO, "update Password operation...\n");
    syslog(LOG_INFO, "oldPassword: %s\n", req->GetString("oldPassword").c_str());
    syslog(LOG_INFO, "newPassword: %s\n", req->GetString("newPassword").c_str());

    if (verifyPasswd(req->GetString("oldPassword")) != 0) {
        hv::Json User;
        User["code"] = -1;
        User["msg"] = "Incorrect password";
        User["data"] = hv::Json({});
        return resp->Json(User);
    }

    if (req->GetString("oldPassword") == req->GetString("newPassword")) {
        hv::Json User;
        User["code"] = -1;
        User["msg"] = "Password duplication";
        User["data"] = hv::Json({});
        return resp->Json(User);
    }

    FILE* fp;
    char info[128];
    char cmd[128] = SCRIPT_USER_PWD;

    strcat(cmd, g_sUserName.c_str());
    strcat(cmd, " ");
    strcat(cmd, req->GetString("oldPassword").c_str());
    strcat(cmd, " ");
    strcat(cmd, req->GetString("newPassword").c_str());
    syslog(LOG_DEBUG, "cmd: %s\n", cmd);

    fp = popen(cmd, "r");
    if (fp == NULL) {
        syslog(LOG_ERR, "Failed to run `%s`(%s)\n", cmd, strerror(errno));
        return -1;
    }

    hv::Json User;
    User["code"] = 0;
    User["msg"] = "";
    User["data"] = hv::Json({});

    if (fgets(info, sizeof(info) - 1, fp) != NULL) {
        syslog(LOG_INFO, "info: =%s=\n", info);
        if (strcmp(info, "OK\n") != 0) {
            syslog(LOG_WARNING, "error here\n");
            User["code"] = 1109;
            if (fgets(info, sizeof(info) - 1, fp) != NULL) {
                syslog(LOG_WARNING, "error info: %s\n", info);
                User["msg"] = std::string(info);
            }
        } else {
            savePasswd(req->GetString("newPassword"));
        }
    }
    pclose(fp);

    return resp->Json(User);
}

int addSShkey(HttpRequest* req, HttpResponse* resp)
{
    syslog(LOG_INFO, "Add ssh key operation...\n");
    syslog(LOG_INFO, "name: %s\n", req->GetString("name").c_str());
    syslog(LOG_INFO, "value: %s\n", req->GetString("value").c_str());

    hv::Json response;
    std::ofstream file;
    std::string keyValue = req->GetString("value");

    if(0 != verifyKey(keyValue)) {
        response["code"] = -1;
        response["msg"] = "Invalid key";
        response["data"] = hv::Json({});
        return resp->Json(response);
    }

    file.open(PATH_SSH_KEY_FILE, std::ios_base::app);
    if (!file.is_open()) {
        syslog(LOG_ERR, "open %s file failed here!\n", PATH_SSH_KEY_FILE);
        response["code"] = -1;
        response["msg"] = "Secret key file does not exist";
        response["data"] = hv::Json({});
        return resp->Json(response);
    }

    file << keyValue << "#" << " " << req->GetString("name") << " " << req->GetString("time") << "\n";
    file.close();

    response["code"] = 0;
    response["msg"] = "";
    response["data"] = hv::Json({});

    return resp->Json(response);
}

int deleteSShkey(HttpRequest* req, HttpResponse* resp)
{
    syslog(LOG_INFO, "delete ssh key operation...\n");
    syslog(LOG_INFO, "id: %s\n", req->GetString("id").c_str());

    char cmd[128] = "";

    strcat(cmd, "sed -i \'");
    strcat(cmd, req->GetString("id").c_str());
    strcat(cmd, "d\' ");
    strcat(cmd, PATH_SSH_KEY_FILE);
    system(cmd);

    hv::Json response;
    response["code"] = 0;
    response["msg"] = "";
    response["data"] = hv::Json({});

    return resp->Json(response);
}
