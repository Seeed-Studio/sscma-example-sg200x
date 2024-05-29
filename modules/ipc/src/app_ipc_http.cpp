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
#include "app_ipcam_comm.h"

using namespace hv;

EXTERN_C_START()

static HttpServer server;

int app_ipc_Httpd_Init()
{
    static HttpService router;

    /* Static file service */
    // curl -v http://ip:port/
    router.Static("/", WWW(""));

    router.GET("/cgi/get_ws_addr.cgi", [](HttpRequest* req, HttpResponse* resp) {
        static char data[64] = "";
        snprintf(data, sizeof(data), "ws://%s:%d", req->host.c_str(), WS_PORT);
        APP_PROF_LOG_PRINT(LEVEL_INFO, "get_ws_addr: %s\n", data);
        return resp->Data(data, sizeof(data));
    });

    router.GET("/cgi/get_cur_chn.cgi", [](const HttpContextPtr& ctx) {
        hv::Json resp;
        resp["current_chn"] = 1;
        printf("get_cur_chn\n");
        return ctx->send(resp.dump(2));
    });

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
