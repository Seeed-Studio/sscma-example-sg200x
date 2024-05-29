#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <net/if.h>
#include <net/route.h>
#include <netinet/in.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include <iostream>
#include <string>

#include "HttpServer.h"
#include "hasync.h" // import hv::async
#include "hthread.h" // import hv_gettid
#include "hv.h"

#include "app_ipc_http.h"
#include "app_ipc_user.h"
#include "app_ipc_wifi.h"
#include "app_ipc_device.h"
#include "app_ipcam_comm.h"

using namespace hv;

EXTERN_C_START()

static HttpServer server;

static void register_Httpd_Redirect(HttpService& router)
{
    router.GET("/generate*", [](HttpRequest* req, HttpResponse* resp) { // android
        // resp->File(WWW("err.html"));
        std::cout << "\n[/generate*]current url: " << req->Url() << "\n";
        std::cout << "-> redirect to " << REDIRECT_URL << "\n";

        return resp->Redirect(REDIRECT_URL);
    });

    router.GET("/*.txt", [](HttpRequest* req, HttpResponse* resp) { // windows
        // resp->File(WWW("err.html"));
        std::cout << "\n[/*.txt]current url: " << req->Url() << "\n";
        std::cout << "-> redirect to \n"
                  << REDIRECT_URL << REDIRECT_URL << "\n";

        return resp->Redirect(REDIRECT_URL);
    });

    router.GET("/index.html", [](HttpRequest* req, HttpResponse* resp) {
        return resp->File(WWW("index.html"));
    });
}

static void register_User_Api(HttpService &router) {
    router.GET("/api/userMgr/queryUserInfo", queryUserInfo);
    router.POST("/api/userMgr/updateUserName", updateUserName);
    router.POST("/api/userMgr/updatePassword", updatePassword);
    router.POST("/api/userMgr/addSShkey", addSShkey);
    router.POST("/api/userMgr/deleteSShkey", deleteSShkey);
}

static void register_WiFi_Api(HttpService &router) {
    router.GET("/api/wifiMgr/queryWiFiInfo", queryWiFiInfo);
    router.POST("/api/wifiMgr/scanWiFi", scanWiFi);
    router.POST("/api/wifiMgr/connectWiFi", connectWiFi);
    router.POST("/api/wifiMgr/disconnectWiFi", disconnectWiFi);
    router.POST("/api/wifiMgr/switchWiFi", switchWiFi);
    router.GET("/api/wifiMgr/getWifiStatus", getWifiStatus);
    router.POST("/api/wifiMgr/autoConnectWiFi", autoConnectWiFi);
    router.POST("/api/wifiMgr/forgetWiFi", forgetWiFi);
}

static void register_Device_Api(HttpService &router) {
    router.GET("/api/deviceMgr/queryDeviceInfo", queryDeviceInfo);
    router.POST("/api/deviceMgr/updateDeviceName", updateDeviceName);
    router.POST("/api/deviceMgr/updateChannel", updateChannel);
    router.POST("/api/deviceMgr/setPower", setPower);
    router.POST("/api/deviceMgr/updateSystem", updateSystem);
    router.GET("/api/deviceMgr/getUpdateProgress", getUpdateProgress);
    router.POST("/api/deviceMgr/cancelUpdate", cancelUpdate);
}


static void register_WebSocket(HttpService& router)
{
    router.GET("/cgi/get_ws_addr.cgi", [](HttpRequest* req, HttpResponse* resp) {
        static char data[64] = "";
        snprintf(data, sizeof(data), "ws://%s:%d", req->host.c_str(), WS_PORT);
        APP_PROF_LOG_PRINT(LEVEL_INFO, "get_ws_addr: %s\n", data);
        return resp->Data(data, sizeof(data));
    });
}

int app_ipc_Httpd_Init()
{
    static HttpService router;

    /* Static file service */
    // curl -v http://ip:port/
    router.Static("/", WWW(""));

    register_Httpd_Redirect(router);
    register_User_Api(router);
    register_WiFi_Api(router);
    register_Device_Api(router);
    register_WebSocket(router);

    server.service = &router;
    server.port = HTTPD_PORT;
    server.start();

    return 0;
}

int app_ipc_Httpd_DeInit()
{
    server.stop();
    hv::async::cleanup();
    return 0;
}

EXTERN_C_END()
