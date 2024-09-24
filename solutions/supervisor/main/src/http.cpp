#include <iostream>
#include <string>
#include <syslog.h>

#include "hv/HttpServer.h"
#include "hv/hthread.h"    // import hv_gettid
#include "hv/hasync.h"     // import hv::async

#include "http.h"
#include "utils_device.h"
#include "utils_user.h"
#include "utils_wifi.h"

using namespace hv;

extern "C" {

static HttpServer server;

static void registerHttpRedirect(HttpService& router)
{
    router.GET("/hotspot-detect*", [](HttpRequest* req, HttpResponse* resp) { // IOS
        syslog(LOG_DEBUG, "[/hotspot-detect*]current url: %s -> redirect to %s\n", req->Url().c_str(), REDIRECT_URL);

        return resp->Redirect(REDIRECT_URL);
    });

    router.GET("/generate*", [](HttpRequest* req, HttpResponse* resp) { // android
        syslog(LOG_DEBUG, "[/generate*]current url: %s -> redirect to %s\n", req->Url().c_str(), REDIRECT_URL);

        return resp->Redirect(REDIRECT_URL);
    });

    router.GET("/*.txt", [](HttpRequest* req, HttpResponse* resp) { // windows
        syslog(LOG_DEBUG, "[/*.txt]current url: %s -> redirect to %s\n", req->Url().c_str(), REDIRECT_URL);

        return resp->Redirect(REDIRECT_URL);
    });

    router.GET("/index.html", [](HttpRequest* req, HttpResponse* resp) {
        return resp->File(WWW("index.html"));
    });
}

static void registerUserApi(HttpService& router)
{
    router.GET("/api/userMgr/queryUserInfo", queryUserInfo);
    router.POST("/api/userMgr/updateUserName", updateUserName);
    router.POST("/api/userMgr/updatePassword", updatePassword);
    router.POST("/api/userMgr/addSShkey", addSShkey);
    router.POST("/api/userMgr/deleteSShkey", deleteSShkey);
}

static void registerWiFiApi(HttpService& router)
{
    router.GET("/api/wifiMgr/queryWiFiInfo", queryWiFiInfo);
    router.POST("/api/wifiMgr/scanWiFi", scanWiFi);
    router.POST("/api/wifiMgr/connectWiFi", connectWiFi);
    router.POST("/api/wifiMgr/disconnectWiFi", disconnectWiFi);
    router.POST("/api/wifiMgr/switchWiFi", switchWiFi);
    router.GET("/api/wifiMgr/getWifiStatus", getWifiStatus);
    router.POST("/api/wifiMgr/autoConnectWiFi", autoConnectWiFi);
    router.POST("/api/wifiMgr/forgetWiFi", forgetWiFi);
}

static void registerDeviceApi(HttpService& router)
{
    router.GET("/api/deviceMgr/getSystemStatus", getSystemStatus);
    router.POST("/api/deviceMgr/getSystemUpdateVesionInfo", getSystemUpdateVesionInfo);
    router.GET("/api/deviceMgr/queryDeviceInfo", queryDeviceInfo);
    router.POST("/api/deviceMgr/updateDeviceName", updateDeviceName);
    router.POST("/api/deviceMgr/updateChannel", updateChannel);
    router.POST("/api/deviceMgr/setPower", setPower);
    router.POST("/api/deviceMgr/updateSystem", updateSystem);
    router.GET("/api/deviceMgr/getUpdateProgress", getUpdateProgress);
    router.POST("/api/deviceMgr/cancelUpdate", cancelUpdate);

    router.GET("/api/deviceMgr/getDeviceList", getDeviceList);
    router.GET("/api/deviceMgr/getDeviceInfo", getDeviceInfo);

    router.GET("/api/deviceMgr/getAppInfo", getAppInfo);
    router.POST("/api/deviceMgr/uploadApp", uploadApp);

    router.GET("/api/deviceMgr/getModelInfo", getModelInfo);
    router.POST("/api/deviceMgr/uploadModel", uploadModel);
}

static void registerWebSocket(HttpService& router)
{
    router.GET("/api/deviceMgr/getCameraWebsocketUrl", [](HttpRequest* req, HttpResponse* resp) {
        hv::Json data;
        data["websocketUrl"] = "ws://" + req->host + ":" + std::to_string(WS_PORT);
        hv::Json res;
        res["code"] = 0;
        res["msg"] = "";
        res["data"] = data;

        std::string s_time = req->GetParam("time");//req->GetString("time");
        int64_t time = std::stoll(s_time);
        time /= 1000; // to sec
        std::string cmd = "date -s @" + std::to_string(time);
        system(cmd.c_str());

        syslog(LOG_INFO, "WebSocket: %s\n", data["websocketUrl"]);
        return resp->Json(res);
    });
}

int initHttpd()
{
    static HttpService router;

    router.AllowCORS();
    router.Static("/", WWW(""));

    registerHttpRedirect(router);
    registerUserApi(router);
    registerWiFiApi(router);
    registerDeviceApi(router);
    registerWebSocket(router);

    server.service = &router;
    server.port = HTTPD_PORT;
    server.start();

    return 0;
}

int deinitHttpd()
{
    server.stop();
    hv::async::cleanup();
    return 0;
}

int initWiFi() {
    char cmd[128] = SCRIPT_WIFI_START;
    std::string wifiName = getWiFiName("wlan0");

    initUserInfo();

    strcat(cmd, wifiName.c_str());
    strcat(cmd, " ");
    strcat(cmd, std::to_string(TTYD_PORT).c_str());
    strcat(cmd, " ");
    strcat(cmd, std::to_string(g_userId).c_str());
    system(cmd);

    return 0;
}

} // extern "C" {
