#include <stdio.h>
#include <iostream>
#include "HttpServer.h"
#include "app_ipc_device.h"

std::string g_sDeviceName = "reCamera";
std::string g_serverUrl = "https://github.com/Seeed-Studio/reCamera";
int g_channel = 0;
int g_progress = 0;

int queryDeviceInfo(HttpRequest* req, HttpResponse* resp) {

    hv::Json device;
    device["code"] = 0;
    device["msg"] = "";

    hv::Json data;
    data["deviceName"] = g_sDeviceName;
    data["ip"] = "192.168.120.99";
    data["mask"] = "255.255.255.0";
    data["gateway"] = "192.168.120.1";
    data["dns"] = "192.168.120.1";
    data["channel"] = g_channel;
    data["serverUrl"] = g_serverUrl;
    data["cpu"] = "sg2002";
    data["ram"] = 128;
    data["npu"] = 2;
    data["osName"] = "reCamera";
    data["osVersion"] = "1.0.0";

    device["data"] = data;

    return resp->Json(device);
}

int updateDeviceName(HttpRequest* req, HttpResponse* resp) {

    std::cout << "\nupdate Device Name operation...\n";
    std::cout << "deviceName: " << req->GetString("deviceName") << "\n";

    g_sDeviceName = req->GetString("deviceName");

    hv::Json response;

    response["code"] = 0;
    response["msg"] = "";
    response["data"] = hv::Json({});

    return resp->Json(response);
}

int updateChannel(HttpRequest* req, HttpResponse* resp) {

    hv::Json response;
    std::cout << "\nupdate channel operation...\n";
    std::cout << "channel: " << req->GetString("channel") << "\n";
    std::cout << "serverUrl: " << req->GetString("serverUrl") << "\n";

    if (req->GetString("channel").empty()) {
        response["code"] = 1109;
        response["msg"] = "value error";
        response["data"] = hv::Json({});

        return resp->Json(response);
    }

    g_channel = std::stoi(req->GetString("channel"));

    if (!g_channel) {
        g_serverUrl = req->GetString("serverUrl");
    }

    response["code"] = 0;
    response["msg"] = "";
    response["data"] = hv::Json({});

    return resp->Json(response);
}


int setPower(HttpRequest* req, HttpResponse* resp) {

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

int updateSystem(HttpRequest* req, HttpResponse* resp) {

    std::cout << "\nstart to update System now...\n";

    // TODO

    g_progress = 0;

    hv::Json response;

    response["code"] = 0;
    response["msg"] = "";
    response["data"] = hv::Json({});

    return resp->Json(response);
}

int getUpdateProgress(HttpRequest* req, HttpResponse* resp) {

    std::cout << "\nget Update Progress...\n";

    // TODO

    hv::Json response;

    response["code"] = 0;
    response["msg"] = "";

    hv::Json data;
    data["progress"] = g_progress;
    if (g_progress < 100) g_progress += 10;
    response["data"] = data;

    return resp->Json(response);
}

int cancelUpdate(HttpRequest* req, HttpResponse* resp) {

    std::cout << "\ncancel update...\n";

    // TODO
    g_progress = 0;

    hv::Json response;

    response["code"] = 0;
    response["msg"] = "";
    response["data"] = hv::Json({});

    return resp->Json(response);
}
