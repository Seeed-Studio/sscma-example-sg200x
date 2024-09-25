#include <stdio.h>
#include <string>
#include <vector>
#include <syslog.h>
#include <dirent.h>

#include "hv/HttpServer.h"
#include "global_cfg.h"
#include "utils_file.h"

int queryFileList(HttpRequest* req, HttpResponse* resp) {
    DIR* dir;
    struct dirent* ent;
    char fullpath[128];
    hv::Json response;
    std::string folderPath = req->GetString("folderPath");
    std::vector<std::string> fileList;

    if (folderPath.empty()) {
        folderPath = req->GetParam("folderPath");
    }

    dir = opendir(folderPath.c_str());
    if (dir == NULL) {
        response["code"] = -1;
        response["msg"] = "open folder failed";
        response["data"] = hv::Json({});
        syslog(LOG_ERR, "open %s folder failed here\n", folderPath.c_str());

        return resp->Json(response);
    }

    while((ent = readdir(dir)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
            continue;
        }

        snprintf(fullpath, sizeof(fullpath), "%s/%s", folderPath.c_str(), ent->d_name);
        fileList.push_back(std::string(ent->d_name));
    }
    closedir(dir);

    response["code"] = 0;
    response["msg"] = "";
    response["data"]["fileList"] = hv::Json(fileList);

    return resp->Json(response);
}

int uploadFile(HttpRequest* req, HttpResponse* resp) {
    std::string filePath = req->GetString("filePath");
    hv::Json response;
    int ret = 0;

    if (filePath.empty()) {
        filePath = req->GetParam("filePath");
    }

    ret = req->SaveFile(filePath.c_str());
    if (200 == ret) {
        response["code"] = 0;
        response["msg"] = "Upload file successfully";
    } else {
        response["code"] = -1;
        response["msg"] = "Upload file failed";
    }
    response["data"] = hv::Json({});

    return resp->Json(response);
}

int deleteFile(HttpRequest* req, HttpResponse* resp) {
    std::string filePath = req->GetString("filePath");
    hv::Json response;
    int ret = 0;

    if (filePath.empty()) {
        filePath = req->GetParam("filePath");
    }

    ret = remove(filePath.c_str());

    if (0 == ret) {
        response["code"] = ret;
        response["msg"] = "Delete file successfully";
    } else {
        response["code"] = ret;
        response["msg"] = "Delete file failed";
    }
    response["data"] = hv::Json({});

    return resp->Json(response);
}
