#ifndef _HTTP_SERVER_H_
#define _HTTP_SERVER_H_
// #include <cerrno>
// #include <memory>

#include "version.h"

#include "hv/HttpServer.h"
#include "hv/hasync.h" // import hv::async
#include "hv/hlog.h"
#include "hv/hthread.h" // import hv_gettid

typedef enum {
    API_TYPE_HEAD = 0,
    API_TYPE_GET,
    API_TYPE_POST,

    API_TYPE_MAX
} api_type_t;

typedef int (*api_func_t)(HttpRequest* req, HttpResponse* resp);
typedef struct {
    api_type_t type;
    const char* api;
    api_func_t func;
} api_t;

// using namespace std;

class http_server {
public:
    http_server(const std::string& resource, uint16_t http_port = 80, uint16_t https_port = 443)
        : resource_(resource)
        , http_port_(http_port)
        , https_port_(https_port)
        , server_(std::make_unique<hv::HttpServer>())
    {
        syslog(LOG_INFO, "http server resource: %s, http: %d, https: %d\n",
            resource.c_str(), http_port, https_port);
        hlog_disable();
    }

    ~http_server()
    {
        if (server_ != nullptr) {
            server_->stop();
            hv::async::cleanup();
        }
    }

    void register_redirect(const std::string& redirect)
    {
        const char* urls[] = {
            "/hotspot-detect*", // iOS
            "/generate*", // android
            "/*.txt", // windows
        };

        if (!redirect.empty()) {
            syslog(LOG_ERR, "redirect: %s\n", redirect.c_str());
            return;
        }

        for (const auto& url : urls) {
            router_.GET(url, [redirect](HttpRequest* req, HttpResponse* resp) {
                syslog(LOG_DEBUG, "req url: %s -> redirect: %s\n", req->Url().c_str(), redirect.c_str());
                return resp->Redirect(redirect.c_str());
            });
        }
    }

    void register_get(api_t* apis)
    {
        for (int i = 0; (nullptr != apis[i].api); ++i) {
            router_.GET(apis[i].api, apis[i].func);
        }
    }

    void register_post(api_t* apis)
    {
        for (int i = 0; (nullptr != apis[i].api); ++i) {
            router_.POST(apis[i].api, apis[i].func);
        }
    }

    void start()
    {
        if (server_ == nullptr) {
            throw std::runtime_error("Server is null");
        }

        router_.AllowCORS();
        router_.Static("/", resource_.c_str());
        // router_.Use(authorization);
        router_.GET("/api/version", [](HttpRequest* req, HttpResponse* resp) {
            hv::Json res;
            res["code"] = 0;
            res["msg"] = "";
            res["data"] = PROJECT_VERSION;
            return resp->Json(res);
        });

        if (https_port_ > 0) {
            // initHttpsService();
        }

        // index.html
        router_.GET("/index.html", [res = std::string(resource_ + "/index.html")](HttpRequest* req, HttpResponse* resp) {
            return resp->File(res.c_str());
        });

        server_->service = &router_;
        server_->port = http_port_;
        server_->start();
    }

private:
    uint16_t http_port_;
    uint16_t https_port_;

    std::string resource_;

    hv::HttpService router_;
    std::unique_ptr<hv::HttpServer> server_;
};

#endif // _HTTP_SERVER_H_
