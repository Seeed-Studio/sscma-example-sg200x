#ifndef HTTP_H
#define HTTP_H

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "mongoose.h"

typedef void (*HttpMethod)(mg_connection* c, mg_http_message* hm);

class HttpRouter {
private:
    static std::vector<HttpRouter*> _routes;
    static std::mutex _routes_mutex;

    const bool _no_auth;
    const char* _uri;
    const HttpMethod _method;

public:
    HttpRouter(const char* uri, HttpMethod method, bool no_auth = false)
        : _uri(uri)
        , _method(method)
        , _no_auth(no_auth)
    {
        printf("%s,%d: %s\n", __func__, __LINE__, _uri);
        std::lock_guard<std::mutex> lock(_routes_mutex);
        _routes.emplace_back(this);
    }

    ~HttpRouter()
    {
        printf("%s,%d: %s\n", __func__, __LINE__, _uri);
    }

    static void handler(mg_connection* c, mg_http_message* hm)
    {
        printf("%s,%d\n", __func__, __LINE__);

        std::lock_guard<std::mutex> lock(_routes_mutex);
        for (auto& route : _routes) {
            printf("%s,%d: %s\n", __func__, __LINE__, route->_uri);

            if (mg_match(hm->uri, mg_str(route->_uri), NULL)) {
                if (!route->_method)
                    continue;
                route->_method(c, hm);
                return;
            }
        }
        mg_http_reply(c, 404, "Content-Type: text/plain\r\n", "Not Found\n");
    }
};

#define HTTP_ROUTE(uri, method, no_auth) \
    const HttpRouter __http_route_##method(uri, method, no_auth)

class HttpServer {
private:
    const char* _cert;
    const char* _key;
    const char* _root_dir;

    mg_mgr mgr;
    mg_connection* http_conn = nullptr;
    mg_connection* https_conn = nullptr;
    std::atomic<bool> running { false };
    std::thread worker;

    static void event_handler(mg_connection* c, int ev, void* ev_data)
    {
        if (ev == MG_EV_HTTP_MSG) {
            mg_http_message* hm = (mg_http_message*)ev_data;
            HttpRouter::handler(c, hm);
        }
    }

    static void https_event_handler(mg_connection* c, int ev, void* ev_data)
    {
        HttpServer* server = static_cast<HttpServer*>(c->fn_data);
        if (ev == MG_EV_ACCEPT) {
            printf("ev: MG_EV_ACCEPT\n");
            struct mg_tls_opts opts;
            memset(&opts, 0, sizeof(opts));
            opts.cert = mg_str(server->_cert);
            opts.key = mg_str(server->_key);
            mg_tls_init(c, &opts);
        } else {
            event_handler(c, ev, ev_data);
        }
    }

    void poll_loop()
    {
        running = true;
        while (running) {
            mg_mgr_poll(&mgr, 50);
        }
        printf("poll_loop exit\n");
    }

public:
    HttpServer(const char* root_dir = "www", const char* cert = nullptr, const char* key = nullptr)
        : _cert(cert)
        , _key(key)
        , _root_dir(root_dir)
    {
        mg_mgr_init(&mgr);
    }

    ~HttpServer()
    {
        stop();
    }

    bool start(const char* http_port = nullptr, const char* https_port = nullptr)
    {
        if (http_port && strlen(http_port) > 0) {
            http_conn = mg_http_listen(&mgr, http_port, event_handler, this);
            if (!http_conn)
                return false;
            printf("HTTP server started on %s\n", http_port);
        }

        if (https_port && *https_port) {
            https_conn = mg_http_listen(&mgr, https_port, https_event_handler, this);
            if (!https_conn)
                return false;
            printf("HTTPS server started on %s\n", https_port);
        }

        if (!http_conn && !https_conn) {
            printf("Error: At least one valid port required\n");
            return false;
        }

        worker = std::thread(&HttpServer::poll_loop, this);
        return true;
    }

    void stop()
    {
        printf("%s,%d: Stopping server\n", __func__, __LINE__);
        if (running) {
            running = false;
            if (worker.joinable()) {
                worker.join();
                printf("%s,%d: Worker thread stopped\n", __func__, __LINE__);
            }
            mg_mgr_free(&mgr);
        }
        printf("%s,%d: Server stopped\n", __func__, __LINE__);
    }
};

#endif
