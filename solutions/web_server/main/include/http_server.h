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
            if (now - it->second < TOKEN_EXPIRATION_TIME) {
                return true;
            } else {
                _tokens.erase(it);
            }
        }

        return false;
    }

    api_status_t api_handler(mg_http_message* hm, json& response)
    {
        api_status_t status = API_STATUS_NEXT;
        rest_api* found_api = nullptr;
        string request_uri(hm->uri.buf, hm->uri.len);

        for (auto& _api : _apis) {
            const string group = "/api/" + _api->_group;
            if (request_uri.compare(0, group.length(), group) != 0) {
                continue;
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
            if (!found_api->_no_auth) {
                mg_str* token = mg_http_get_header(hm, "Authorization");
                if (!token || !check_token(string(token->buf, token->len))) {
                    return API_STATUS_UNAUTHORIZED;
                }
            }

            json request;
            request["uri"] = string(hm->uri.buf, hm->uri.len);
            request["body"] = (hm->body.len) ? json::parse(hm->body.buf) : "";
            status = found_api->_handler(request, response);
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

            // printf("\n\n>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
            // printf("\n%s,%d: uri=[%d]%s", __func__, __LINE__, hm->uri.len, hm->uri.buf);
            // printf("\n%s,%d: head=[%d]%s", __func__, __LINE__, hm->head.len, hm->head.buf);
            // printf("\n%s,%d: body=[%d]%s", __func__, __LINE__, hm->body.len, hm->body.buf);
            // printf("\n%s,%d: message=[%d]%s", __func__, __LINE__, hm->message.len, hm->message.buf);
            // printf("\n<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n\n");

            api_status_t status = API_STATUS_NEXT;
            json response;
            status = server->api_handler(hm, response);
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
