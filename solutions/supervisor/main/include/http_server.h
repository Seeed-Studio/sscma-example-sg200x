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

class api_t {
public:
    bool auth;
    std::string method;
    std::string path;
    http_sync_handler sync;
    http_async_handler async;
    http_ctx_handler ctx;
    http_state_handler state;

    api_t(bool auth_, const std::string& method_, const std::string& path_,
        http_sync_handler sync_,
        http_async_handler async_,
        http_ctx_handler ctx_,
        http_state_handler state_)
        : auth(auth_)
        , method(method_)
        , path(path_)
        , sync(sync_)
        , async(async_)
        , ctx(ctx_)
        , state(state_)
    {
    }

    friend std::ostream& operator<<(std::ostream& os, const api_t& api)
    {
        os << "Auth: " << api.auth << ", Method: " << api.method << ", Path: " << api.path;
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
            if (api.sync)
                router_.Handle<http_sync_handler>(api.method.c_str(), api.path.c_str(), api.sync);
            else if (api.async)
                router_.Handle<http_async_handler>(api.method.c_str(), api.path.c_str(), api.async);
            else if (api.ctx)
                router_.Handle<http_ctx_handler>(api.method.c_str(), api.path.c_str(), api.ctx);
            else if (api.state)
                router_.Handle<http_state_handler>(api.method.c_str(), api.path.c_str(), api.state);
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
