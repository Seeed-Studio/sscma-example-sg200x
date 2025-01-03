#ifndef _HTTP_SERVER_H_
#define _HTTP_SERVER_H_

#include <filesystem>

#include "logger.hpp"
#include "version.h"

#include "hv/HttpServer.h"
#include "hv/HttpService.h"
#include "hv/hasync.h" // import hv::async
#include "hv/hlog.h"
#include "hv/hthread.h" // import hv_gettid

#undef TAG
#define TAG "http_server"

typedef enum {
    API_TYPE_HEAD = 0,
    API_TYPE_GET,
    API_TYPE_POST,

    API_TYPE_MAX
} api_type_t;

class api_t {
public:
    api_type_t type;
    bool auth;
    std::string url;
    std::function<int(HttpRequest*, HttpResponse*)> func;

    api_t(api_type_t t, bool a, const std::string& u, std::function<int(HttpRequest*, HttpResponse*)> f)
        : type(t)
        , auth(a)
        , url(u)
        , func(f)
    {
    }

    friend std::ostream& operator<<(std::ostream& os, const api_t& api)
    {
        os << "Type: " << api.type << ", Auth: " << api.auth << ", Url: " << api.url;
        return os;
    }
};

class http_server {
public:
    http_server(const std::string& resource, const std::string& redirect,
        uint16_t http_port = 80, uint16_t https_port = 443)
        : resource_(resource)
        , redirect_(redirect)
        , http_port_(http_port)
        , https_port_(https_port)
        , server_(std::make_unique<hv::HttpServer>())
    {
        MA_LOGI(TAG, "resource: %s", resource_.c_str());
        MA_LOGI(TAG, "redirect: %s", redirect_.c_str());
        MA_LOGI(TAG, "http port: %d, https port: %d", http_port, https_port);
        hlog_disable();
        set_redirect(redirect_);
    }

    ~http_server()
    {
        if (server_ != nullptr) {
            MA_LOGI(TAG, "exit");
            server_->stop();
            hv::async::cleanup();
        }
    }

    void register_apis(const std::vector<api_t>& apis)
    {
        for (auto& api : apis) {
            switch (api.type) {
            case API_TYPE_HEAD:
                router_.HEAD(api.url.c_str(), api.func);
                break;
            case API_TYPE_GET:
                router_.GET(api.url.c_str(), api.func);
                break;
            case API_TYPE_POST:
                router_.POST(api.url.c_str(), api.func);
                break;
            default:
                throw std::runtime_error("Invalid api type");
            }
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

        server_->service = &router_;
        server_->port = http_port_;
        server_->start();
    }

private:
    uint16_t http_port_;
    uint16_t https_port_;

    std::string resource_;
    std::string redirect_;

    hv::HttpService router_;
    std::unique_ptr<hv::HttpServer> server_;

    void set_redirect(const std::string& redirect)
    {
        if (redirect.empty()) {
            MA_LOGE(TAG, "redirect is empty");
            return;
        }

        router_.GET("/*", [this](HttpRequest* req, HttpResponse* resp) {
            std::string req_url = req->Url();
            MA_LOGD(TAG, "Request url: %s", req_url.c_str());
            size_t s_pos = req_url.find("//");
            if (s_pos != std::string::npos) {
                size_t e_pos = req_url.find("/", s_pos + 2);
                if (e_pos == std::string::npos)
                    goto goto_redirect;

                std::string res_path = this->resource_ + req_url.substr(e_pos);
                if (std::filesystem::exists(res_path)) {
                    MA_LOGD(TAG, "=====> path: %s", res_path.c_str());
                    return resp->File(res_path.c_str());
                }
            }

        goto_redirect:
            std::string res_path = redirect_;
            MA_LOGD(TAG, "-----> redirect: %s", res_path.c_str());
            return resp->Redirect(res_path.c_str());
        });
    }
};

#endif // _HTTP_SERVER_H_
