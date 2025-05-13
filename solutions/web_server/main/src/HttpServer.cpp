#include "HttpServer.h"

std::mutex HttpRouter::_routes_mutex;
std::vector<HttpRouter*> HttpRouter::_routes;

void api(mg_connection* c, mg_http_message* hm)
{
    printf("%s,%d\n", __func__, __LINE__);
    mg_http_reply(c, 404, "Content-Type: text/plain\r\n", "api1\n");
}

void api2(mg_connection* c, mg_http_message* hm)
{
    printf("%s,%d\n", __func__, __LINE__);
    mg_http_reply(c, 404, "Content-Type: text/plain\r\n", "api2\n");
}

HTTP_ROUTE("/api", api, true);
HTTP_ROUTE("/api2", api2, true);