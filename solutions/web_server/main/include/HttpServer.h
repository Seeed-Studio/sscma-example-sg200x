#ifndef HTTP_H
#define HTTP_H

#include <atomic>
#include <thread>
#include <string.h>

#include "mongoose.h"

class HttpServer {
private:
    const char* _cert;
    const char* _key;
    mg_mgr mgr;
    mg_connection *http_conn = nullptr;
    mg_connection *https_conn = nullptr;
    mg_http_serve_opts opts{};
    std::atomic<bool> running{false};
    std::thread worker;

    static void event_handler(mg_connection *c, int ev, void *ev_data) {
        switch (ev) {
        case MG_EV_HTTP_MSG:
        {
            printf("ev: MG_EV_HTTP_MSG\n");
            mg_http_message *hm = (mg_http_message *)ev_data;
            mg_http_reply(c, 200, "Content-Type: application/json\r\n", "{\"status\":\"OK\"}");
        }
        break;
        }
    }

    static void https_event_handler(mg_connection *c, int ev, void *ev_data) {
        HttpServer* server = static_cast<HttpServer*>(c->fn_data);
        if (ev == MG_EV_ACCEPT) {
            printf("ev: MG_EV_ACCEPT\n");
            struct mg_tls_opts opts;
            memset(&opts, 0, sizeof(opts));
            opts.cert = mg_str(server->_cert);
            opts.key = mg_str(server->_key);
            mg_tls_init(c, &opts);
        }
        else {
            event_handler(c, ev, ev_data);
        }
    }

    void poll_loop() {
        while (running) {
            mg_mgr_poll(&mgr, 50);
        }
        printf("poll_loop exit\n");
    }

public:
    HttpServer(const char *root_dir = "www", const char* cert = nullptr, const char* key = nullptr) 
        : _cert(cert), _key(key), opts{.root_dir = root_dir} {
        mg_mgr_init(&mgr);
        opts.extra_headers = "Cache-Control: no-cache\r\n";
    }

    ~HttpServer() {
        stop();
    }

    bool start(const char *http_port = nullptr, const char *https_port = nullptr) {
        if (http_port && strlen(http_port) > 0) {
            http_conn = mg_http_listen(&mgr, http_port, event_handler, this);
            if (!http_conn) return false;
            printf("HTTP server started on %s\n", http_port);
        }

        if (https_port && *https_port) {
            https_conn = mg_http_listen(&mgr, https_port, https_event_handler, this);
            if (!https_conn) return false;
            printf("HTTPS server started on %s\n", https_port);
        }

        if (!http_conn && !https_conn) {
            printf("Error: At least one valid port required\n");
            return false;
        }

        running = true;
        worker = std::thread(&HttpServer::poll_loop, this);
        return true;
    }

    void stop() {
        printf("Stopping server\n");
        if (running) {
            running = false;
            if (worker.joinable()) worker.join();
            mg_mgr_free(&mgr);
            printf("Server stopped\n");
        }
    }
};

#endif
