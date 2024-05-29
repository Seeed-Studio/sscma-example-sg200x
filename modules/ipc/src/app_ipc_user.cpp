#include <stdio.h>
#include <iostream>
#include <vector>
#include <unistd.h>
#include <pwd.h>
#include "HttpServer.h"
#include "app_ipc_user.h"

std::string g_sUserName;
std::string g_sPassword;

int getUserName() {
    char username[16];
    struct passwd *pw = getpwuid(getuid());

    if (pw) {
        strncpy(username, pw->pw_name, sizeof(username));
    } else {
        perror("getpwuid");
        return 1;
    }

    g_sUserName.assign(username);
}

int queryUserInfo(HttpRequest* req, HttpResponse* resp) {

    hv::Json User;
    User["code"] = 0;
    User["msg"] = "";

    getUserName();

    hv::Json data;
    data["userName"] = g_sUserName;
    data["password"] = g_sPassword;

    FILE *fp;
    char info[128];
    std::vector<hv::Json> sshkeyList;

    fp = popen("/mnt/usertool.sh query_key", "r");
    if (fp == NULL) {
        printf("Failed to run /mnt/usertool.sh query_key\n");
    }

    while (fgets(info, sizeof(info) - 1, fp) != NULL) {
        std::string s(info);
        hv::Json sshkey;
        if (s.back() == '\n') {
            s.erase(s.size() - 1);
        }

        sshkey["id"] = "-";
        sshkey["name"] = "-";
        sshkey["value"] = s;
        sshkey["addTime"] = "20240101";
        sshkey["latestUsedTime"] = "20240101";
        sshkeyList.push_back(sshkey);
    }

    fclose(fp);

    data["sshkeyList"] = hv::Json(sshkeyList);
    User["data"] = data;

    // std::cout << "result: " << User.dump(2) << "\n";

    return resp->Json(User);
}

int updateUserName(HttpRequest* req, HttpResponse* resp) {

    std::cout << "\nupdate UserName operation...\n";
    std::cout << "userName: " << req->GetString("userName") << "\n";


    FILE *fp;
    char info[128];
    char cmd[128] = "/mnt/usertool.sh name ";

    strcat(cmd, g_sUserName.c_str());
    strcat(cmd, " ");
    strcat(cmd, req->GetString("userName").c_str());

    printf("cmd: %s\n", cmd);
    fp = popen(cmd, "r");
    if (fp == NULL) {
        printf("Failed to run %s list\n", cmd);
        return -1;
    }

    hv::Json User;
    User["code"] = 0;
    User["msg"] = "";
    User["data"] = hv::Json({});

    return resp->Json(User);
}

int updatePassword(HttpRequest* req, HttpResponse* resp) {

    std::cout << "\nupdate Password operation...\n";
    std::cout << "oldPassword: " << req->GetString("oldPassword") << "\n";
    std::cout << "newPassword: " << req->GetString("newPassword") << "\n";

    FILE *fp;
    char info[128];
    char cmd[128] = "/mnt/usertool.sh passwd ";

    strcat(cmd, req->GetString("oldPassword").c_str());
    strcat(cmd, " ");
    strcat(cmd, req->GetString("newPassword").c_str());

    printf("cmd: %s\n", cmd);
    fp = popen(cmd, "r");
    if (fp == NULL) {
        printf("Failed to run %s list\n", cmd);
        return -1;
    }

    hv::Json User;
    User["code"] = 0;
    User["msg"] = "";
    User["data"] = hv::Json({});

    if (fgets(info, sizeof(info) - 1, fp) != NULL) {
        printf("info: =%s=\n", info);
        if (strcmp(info, "OK\n") != 0) {
            printf("error here\n");
            User["code"] = 1109;
            if (fgets(info, sizeof(info) - 1, fp) != NULL) {
                printf("error info: %s\n", info);
                User["msg"] = std::string(info);
            }
        }
    }

    return resp->Json(User);
}

int addSShkey(HttpRequest* req, HttpResponse* resp) {

    std::cout << "\nAdd ssh key operation...\n";
    std::cout << "name:" << req->GetString("name") << "\n";
    std::cout << "value: " << req->GetString("value") << "\n";

    hv::Json response;
    response["code"] = 0;
    response["msg"] = "";

    hv::Json data;
    data["id"] = "666";
    response["data"] = data;

    return resp->Json(response);
}

int deleteSShkey(HttpRequest* req, HttpResponse* resp) {

    std::cout << "\ndelete ssh key operation...\n";
    std::cout << "id: " << req->GetString("id") << "\n";

    hv::Json response;
    response["code"] = 0;
    response["msg"] = "";
    response["data"] = hv::Json({});

    return resp->Json(response);
}
