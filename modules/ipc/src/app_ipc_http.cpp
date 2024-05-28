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

#include "app_ipc_http.h"
#include "hv.h"

#include "HttpServer.h"
#include "hasync.h" // import hv::async
#include "hthread.h" // import hv_gettid
#include <iostream>
#include <string>

using namespace hv;

EXTERN_C_START()

static HttpServer server;
int app_ipc_Httpd_Init()
{
    static HttpService router;

    /* Static file service */
    // curl -v http://ip:port/
    router.Static("/", WWW(""));

    router.GET("/index.html", [](HttpRequest* req, HttpResponse* resp) {
        return resp->File(WWW("index.html"));
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
