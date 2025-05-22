#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

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

// template<typename K, typename V>
// void print_map(std::unordered_map<K, V> const &m)
// {
//     for (auto it = m.cbegin(); it != m.cend(); ++it) {
//         std::cout << "{" << (*it).first << ": " << (*it).second << "}\n";
//     }
// }

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
    std::mutex _token_mutex;

    bool check_token(string token)
    {
        std::lock_guard<std::mutex> lock(_token_mutex);

        auto it = _tokens.find(token);
        if (it != _tokens.end()) {
            std::time_t now = std::time(nullptr);
            MA_LOGV(now - it->second);
            if (now - it->second < TOKEN_EXPIRATION_TIME) {
                return true;
            } else {
                _tokens.erase(it);
            }
        }

        return false;
    }

    api_status_t api_handler(mg_http_message* hm, json& res)
    {
        api_status_t status = API_STATUS_NEXT;
        rest_api* found_api = nullptr;
        string req_uri(hm->uri.buf, hm->uri.len);
        string group;

        for (auto& _api : _apis) {
            group = "/api/" + _api->_group;
            if (req_uri.compare(0, group.length(), group) != 0) {
                continue;
            }

            if (_api->_group.empty()) {
                group = "/api";
            }

            for (auto& li : _api->get_list()) {
                const string full_uri = li->_uri.empty() ? group : group + "/" + li->_uri;
                if (mg_match(hm->uri, mg_str(full_uri.c_str()), nullptr)) {
                    found_api = li.get();
                    goto api_found;
                }
            }
        }

    api_found:
        if (found_api && found_api->_handler) {
            // if (!found_api->_no_auth) {
            if (0) { // for testing
                mg_str* token = mg_http_get_header(hm, "Authorization");
                if (!token || !check_token(string(token->buf, token->len))) {
                    return API_STATUS_UNAUTHORIZED;
                }
            }

            status = found_api->_handler(hm, res);
        }

        if (status == API_STATUS_AUTHORIZED) {
            string token = res["data"]["token"];
            res["data"]["expire"] = TOKEN_EXPIRATION_TIME;
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

            MA_LOGV(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>");
            MA_LOGV("---> uri=", string(hm->uri.buf, hm->uri.len));
            MA_LOGV("---> query=", string(hm->query.buf, hm->query.len));
            MA_LOGV("---> head=", string(hm->head.buf, hm->head.len));
            // MA_LOGV("---> body=", string(hm->body.buf, hm->body.len));
            // MA_LOGV(string(hm->message.buf, hm->message.len));
            MA_LOGV("<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n\n\n");

            api_status_t status = API_STATUS_NEXT;
            json res;
            status = server->api_handler(hm, res);
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

    void poll_loop()
    {
        running = true;
        while (running) {
            mg_mgr_poll(&mgr, 50);
        }
        MA_LOGV("poll_loop exit");
    }

public:
    static inline const int TOKEN_EXPIRATION_TIME = 60 * 60 * 24 * 3; // 3 days

    http_server(const char* root_dir = "www", const char* cert = nullptr, const char* key = nullptr)
        : _cert(cert)
        , _key(key)
        , _root_dir(root_dir)
    {
        _apis.emplace_back(make_unique<api_device>());
        _apis.emplace_back(make_unique<api_file>("/userdata/app/"));
        _apis.emplace_back(make_unique<api_led>());
        _apis.emplace_back(make_unique<api_user>());
        _apis.emplace_back(make_unique<api_wifi>());
        _apis.emplace_back(make_unique<api_base>("", "/userdata/main.sh"));
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

        worker = thread(&http_server::poll_loop, this);
        return true;
    }

    void stop()
    {
        MA_LOGV("Stopping server");
        if (running) {
            running = false;
            if (worker.joinable()) {
                worker.join();
                MA_LOGV("Worker thread stopped");
            }
            mg_mgr_free(&mgr);
        }
        MA_LOGV("Server stopped");
    }
};

#endif // HTTP_SERVER_H
