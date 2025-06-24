#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "api_base.h"
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "api_device.h"
#include "api_file.h"
#include "api_led.h"
#include "api_user.h"
#include "api_wifi.h"
#include "mongoose.h"

class http_server {
public:
    http_server(const char* root_dir = "www",
        const char* cert = nullptr, const char* key = nullptr)
        : _cert(cert)
        , _key(key)
        , _root_dir(root_dir)
    {
        _apis.emplace_back(make_unique<api_device>());
        _apis.emplace_back(make_unique<api_file>());
        _apis.emplace_back(make_unique<api_led>());
        _apis.emplace_back(make_unique<api_user>());
        _apis.emplace_back(make_unique<api_wifi>());
        _apis.emplace_back(make_unique<api_base>("", "/userdata/app/main.sh"));
        mg_mgr_init(&mgr);
    }

    ~http_server()
    {
        stop();
    }

    bool start(const string& http_port = "80", const string& https_port = "")
    {
        if (!http_port.empty()) {
            http_conn = mg_http_listen(&mgr, string(":" + http_port).c_str(),
                event_handler, this);
            if (!http_conn)
                return false;
            MA_LOGV("HTTP server started on ", http_port);
        }
        if (!https_port.empty()) {
            https_conn = mg_http_listen(&mgr, string(":" + https_port).c_str(),
                https_event_handler, this);
            if (!https_conn)
                return false;
            MA_LOGV("HTTPS server started on ", https_port);
        }

        if (!http_conn && !https_conn) {
            MA_LOGV("Error: At least one valid port required");
            return false;
        }

        worker = thread([this]() {
            running = true;
            while (running)
                mg_mgr_poll(&mgr, 50);
            MA_LOGV("poll_loop exit");
        });

        return true;
    }

    void stop()
    {
        if (running) {
            running = false;
            if (worker.joinable()) {
                worker.join();
            }
            mg_mgr_free(&mgr);
        }
        MA_LOGV("Server stopped");
    }

private:
    const char* _cert;
    const char* _key;
    const char* _root_dir;
    const char* _ssi_pattern = "#.html";

    mg_mgr mgr;
    mg_connection* http_conn = nullptr;
    mg_connection* https_conn = nullptr;
    atomic<bool> running { false };
    thread worker;

    vector<unique_ptr<api_base>> _apis;

    static void event_handler(mg_connection* c, int ev, void* ev_data)
    {
        if (ev == MG_EV_HTTP_MSG) {
            http_server* server = static_cast<http_server*>(c->fn_data);
            mg_http_message* hm = (mg_http_message*)ev_data;

            MA_LOGV(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>");
            MA_LOGV("---> uri=", string(hm->uri.buf, hm->uri.len));
            MA_LOGV("---> query=", string(hm->query.buf, hm->query.len));
            MA_LOGV("---> head=", string(hm->head.buf, hm->head.len));
            // MA_LOGV("---> body=", string(hm->body.buf, hm->body.len));
            // MA_LOGV(string(hm->message.buf, hm->message.len));
            MA_LOGV("<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n\n\n");

            json res;
            api_status_t status = api_base::api_handler(hm, res);
            if (status == API_STATUS_OK) {
                mg_http_reply(c, 200, "Content-Type: application/json\r\n", res.dump().c_str());
                return;
            } else if (status == API_STATUS_UNAUTHORIZED) {
                mg_http_reply(c, 401, "Content-Type: text/plain\r\n", "Unauthorized");
                return;
            } else if (status != API_STATUS_NEXT) {
                mg_http_reply(c, 500, "Content-Type: text/plain\r\n", "Internal Server Error");
                return;
            }

            // Serve web root directory
            struct mg_http_serve_opts opts = { 0 };
            opts.root_dir = server->_root_dir;
            // opts.ssi_pattern = server->_ssi_pattern;
            mg_http_serve_dir(c, hm, &opts);
        }
    }

    static void https_event_handler(mg_connection* c, int ev, void* ev_data)
    {
        http_server* server = static_cast<http_server*>(c->fn_data);
        if (ev == MG_EV_ACCEPT) {
            struct mg_tls_opts opts;
            memset(&opts, 0, sizeof(opts));
            opts.cert = mg_str(server->_cert);
            opts.key = mg_str(server->_key);
            mg_tls_init(c, &opts);
        } else {
            event_handler(c, ev, ev_data);
        }
    }
};
#endif // HTTP_SERVER_H
