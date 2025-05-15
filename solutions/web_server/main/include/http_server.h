#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <atomic>
#include <iomanip>
#include <memory>
#include <mutex>
#include <sstream>
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

#define TOKEN_EXPIRATION_TIME 60 * 60 * 24 * 3  // 3 days

class http_server {
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
    unordered_map<string, time_t> _tokens;

    bool check_token(string token)
    {
        auto it = _tokens.find(token);
        if (it != _tokens.end()) {
            std::time_t now = std::time(nullptr);
            if (now - it->second < TOKEN_EXPIRATION_TIME) {
                printf("token is valid\n");
                return true;
            } else {
                _tokens.erase(it);
            }
        }

        printf("token is invalid\n");
        return false;
    }

    static bool match(const string &req_uri, const string &api_uri)
    {
        printf("req_uri=%s, api_uri=%s\n", req_uri.c_str(), api_uri.c_str());
        return mg_match(mg_str(req_uri.c_str()), mg_str(api_uri.c_str()), NULL);
    }

    api_status_t api_handler(mg_http_message* hm, json &response)
    {
        api_status_t status = API_STATUS_NEXT;
        if (!mg_match(hm->uri, mg_str("/api/#"), NULL)) {
            return status;
        }

        rest_api* api = nullptr;
        for (auto& _api : _apis) {
            string hm_uri(hm->uri.buf, hm->uri.len);
            api = _api->get(hm_uri, match);
            if (api)
                break;
        }
        if (api && api->_handler) {
            if (!api->_no_auth) {
                mg_str *token = mg_http_get_header(hm, "Authorization");
                if (!token || !check_token(token->buf)) {
                    return API_STATUS_UNAUTHORIZED;
                }
            }

            json request;
            status = api->_handler(request, response);
        }

        if (status == API_STATUS_AUTHORIZED) {
            string token = response["data"]["token"];
            _tokens[token] = std::time(nullptr);
            status = API_STATUS_OK;
        }

        return status;
    }

    static void event_handler(mg_connection* c, int ev, void* ev_data)
    {
        if (ev == MG_EV_HTTP_MSG) {
            http_server* server = static_cast<http_server*>(c->fn_data);
            mg_http_message* hm = (mg_http_message*)ev_data;

            printf("\n\n%s,%d: uri=%s\n", __func__, __LINE__, hm->uri.buf);
            api_status_t status = API_STATUS_NEXT;
            json response;
            status = server->api_handler(hm, response);
            printf("%s,%d: status=%d\n", __func__, __LINE__, status);
            if (status == API_STATUS_OK) {
                mg_http_reply(c, 200, "Content-Type: application/json\r\n", response.dump().c_str());
                return;
            } else if (status == API_STATUS_UNAUTHORIZED) {
                mg_http_reply(c, 401, "Content-Type: text/plain\r\n", "Unauthorized");
                return;
            } else if (status != API_STATUS_NEXT) {
                mg_http_reply(c, 500, "Content-Type: text/plain\r\n", "Internal Server Error");
                return;
            }

            // Serve web root directory
            struct mg_http_serve_opts opts = {0};
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
        _apis.emplace_back(make_unique<api_device>());
        _apis.emplace_back(make_unique<api_file>());
        _apis.emplace_back(make_unique<api_led>());
        _apis.emplace_back(make_unique<api_user>());
        _apis.emplace_back(make_unique<api_wifi>());
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

#endif // HTTP_SERVER_H
