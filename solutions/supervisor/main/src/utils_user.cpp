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
static int g_keyId = 0;

static int getUserName()
{
    char username[16];
    struct passwd* pw = getpwuid(getuid());

    if (pw) {
        strncpy(username, pw->pw_name, sizeof(username));
    } else {
        syslog(LOG_ERR, "getpwuid failed here\n");
        return -1;
    }

    g_sUserName.assign(username);
    return 0;
}

int queryUserInfo(HttpRequest* req, HttpResponse* resp)
{
    hv::Json User;
    User["code"] = 0;
    User["msg"] = "";

    getUserName();
    g_sPassword = readFile(PATH_SECRET);
    if (g_sPassword.back() == '\n') {
        g_sPassword.erase(g_sPassword.size() - 1);
    }
    syslog(LOG_DEBUG, "%s, %s\n", g_sUserName.c_str(), g_sPassword.c_str());

    hv::Json data;
    data["userName"] = g_sUserName;
    data["password"] = g_sPassword;

    FILE* fp;
    char info[128];
    std::vector<hv::Json> sshkeyList;

    fp = popen(SCRIPT_USER_SSH, "r");
    if (fp == NULL) {
        syslog(LOG_ERR, "Failed to run %s\n", SCRIPT_USER_SSH);
    }

    while (fgets(info, sizeof(info) - 1, fp) != NULL) {
        std::vector<std::string> keyInfo;
        std::string s(info);
        if (s.back() == '\n') {
            s.erase(s.size() - 1);
        }

        size_t pos = s.find(' ');
        while (pos < std::string::npos) {
            keyInfo.push_back(s.substr(0, pos));
            s = s.substr(pos + 1);
            pos = s.find(' ');
        }

        hv::Json sshkey;
        if (keyInfo.size() == 3) {
            sshkey["id"] = keyInfo[1];
            g_keyId = std::max(g_keyId, stoi(keyInfo[1]));
            sshkey["name"] = keyInfo[2];
            sshkey["value"] = keyInfo[0];
        } else {
            sshkey["id"] = "-";
            sshkey["name"] = "-";
            sshkey["value"] = "-";
        }
        sshkey["addTime"] = "20240101";
        sshkey["latestUsedTime"] = "20240101";
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
        syslog(LOG_ERR, "Failed to run %s list\n", cmd);
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

    FILE* fp;
    char info[128];
    char cmd[128] = SCRIPT_USER_PWD;

    if (req->GetString("oldPassword").compare(g_sPassword) != 0) {
        hv::Json User;
        User["code"] = 1109;
        User["msg"] = "Incorrect password";
        User["data"] = hv::Json({});
        return resp->Json(User);
    }

    strcat(cmd, req->GetString("oldPassword").c_str());
    strcat(cmd, " ");
    strcat(cmd, req->GetString("newPassword").c_str());
    syslog(LOG_DEBUG, "cmd: %s\n", cmd);

    fp = popen(cmd, "r");
    if (fp == NULL) {
        syslog(LOG_ERR, "Failed to run %s list\n", cmd);
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
            char cmd[128] = "";

            strcat(cmd, "echo ");
            strcat(cmd, req->GetString("newPassword").c_str());
            strcat(cmd, " > ");
            strcat(cmd, PATH_SECRET);

            system(cmd);
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

    std::ofstream file(PATH_SSH_KEY_FILE, std::ios_base::app);

    if (!file.is_open()) {
        syslog(LOG_ERR, "open %s file failed here!\n", PATH_SSH_KEY_FILE);
        return 1;
    }

    file << req->GetString("value") << "#" << ++g_keyId << " " << req->GetString("name") << "\n";
    file.close();

    hv::Json response;
    response["code"] = 0;
    response["msg"] = "";

    hv::Json data;
    data["id"] = g_keyId;
    response["data"] = data;

    return resp->Json(response);
}

int deleteSShkey(HttpRequest* req, HttpResponse* resp)
{
    syslog(LOG_INFO, "delete ssh key operation...\n");
    syslog(LOG_INFO, "id: %s\n", req->GetString("id").c_str());

    char cmd[128];

    strcat(cmd, "sed -i \'/#");
    strcat(cmd, req->GetString("id").c_str());
    strcat(cmd, "/d\' ");
    strcat(cmd, PATH_SSH_KEY_FILE);
    system(cmd);

    hv::Json response;
    response["code"] = 0;
    response["msg"] = "";
    response["data"] = hv::Json({});

    return resp->Json(response);
}
