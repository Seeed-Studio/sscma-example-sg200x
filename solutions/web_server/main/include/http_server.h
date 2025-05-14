#ifndef HTTP_H
#define HTTP_H

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "mongoose.h"

#include "api_device.h"
#include "api_file.h"
#include "api_led.h"
#include "api_user.h"
#include "api_wifi.h"

class http_server {
private:
    const char* _cert;
    const char* _key;
    const char* _root_dir;

    mg_mgr mgr;
    mg_connection* http_conn = nullptr;
    mg_connection* https_conn = nullptr;
    atomic<bool> running { false };
    thread worker;

    vector<unique_ptr<api_base>> apis;

    static void event_handler(mg_connection* c, int ev, void* ev_data)
    {
        if (ev == MG_EV_HTTP_MSG) {
            mg_http_message* hm = (mg_http_message*)ev_data;
            http_server* server = static_cast<http_server*>(c->fn_data);

            for (auto& api : server->apis) {
                if (api->handler(c, hm))
                    return;
            }

            mg_http_reply(c, 404, "Content-Type: text/plain\r\n", "Not Found\n");
        }
    }

    static void https_event_handler(mg_connection* c, int ev, void* ev_data)
    {
        http_server* server = static_cast<http_server*>(c->fn_data);
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
    http_server(const char* root_dir = "www", const char* cert = nullptr, const char* key = nullptr)
        : _cert(cert)
        , _key(key)
        , _root_dir(root_dir)
    {
        apis.emplace_back(make_unique<api_device>());
        apis.emplace_back(make_unique<api_file>());
        apis.emplace_back(make_unique<api_led>());
        apis.emplace_back(make_unique<api_user>());
        apis.emplace_back(make_unique<api_wifi>());
        mg_mgr_init(&mgr);
    }

    ~http_server()
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

        worker = thread(&http_server::poll_loop, this);
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
