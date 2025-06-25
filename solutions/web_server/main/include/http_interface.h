#ifndef _HTTP_INTERFACE_H_
#define _HTTP_INTERFACE_H_

#include "json.hpp"
#include "mongoose.h"

using json = nlohmann::json;

typedef enum {
    API_STATUS_OK = 0,
    API_STATUS_NEXT,
    API_STATUS_ERROR,
    API_STATUS_REPLY_FILE,
    API_STATUS_AUTHORIZED,
    API_STATUS_UNAUTHORIZED,
} api_status_t;
typedef const struct mg_http_message* request_t;
typedef json& response_t;
typedef api_status_t (*api_handler_t)(request_t req, response_t res);

typedef struct {
    std::string name;
    std::string filename;
    char* data;
    size_t len;
} multipart_t;

class http_interface {
public:
    http_interface() = default;
    virtual ~http_interface() = default;

protected:
    static inline const std::string STR_OK = "OK";
    static inline const std::string STR_FAILED = "Failed";

    static void response(response_t res, int code = 0,
        const std::string& msg = STR_OK, const json& data = json({}))
    {
        res["code"] = code;
        res["msg"] = msg;
        res["data"] = data;
    }

    static std::string get_uri(request_t req)
    {
        return std::string(req->uri.buf, req->uri.len);
    }

    static std::string get_host(request_t req, bool ip_only = true)
    {
        std::string host = get_header_var(req, "Host");
        if (ip_only) {
            size_t pos = host.find(':');
            if (pos != std::string::npos) {
                host = host.substr(0, pos);
            }
        }
        return host;
    }

    static std::string get_port(request_t req)
    {
        std::string hdr = get_header_var(req, "Host");
        size_t pos = hdr.find(':');
        return (pos != std::string::npos && pos < hdr.length() - 1)
            ? hdr.substr(pos + 1)
            : "";
    }

    static std::string get_http_var(request_t req, std::string param)
    {
        struct mg_str v = mg_http_var(req->query, mg_str(param.c_str()));
        return std::string(v.buf, v.len);
    }

    static std::string get_header_var(request_t req, const char* name)
    {
        mg_str* hdr = mg_http_get_header((mg_http_message*)req, name);
        return (hdr == nullptr) ? "" : std::string(hdr->buf, hdr->len);
    }

    static json parse_body(request_t req)
    {
        std::string type = get_header_var(req, "Content-Type");
        if (type.find("application/json") == std::string::npos) {
            return json();
        }
        return (req->body.len == 0) ? json() : json::parse(req->body.buf, req->body.buf + req->body.len);
    }

    static std::string get_param(request_t req, std::string param)
    {
        auto&& val = get_http_var(req, param);
        if (!val.empty()) {
            return val;
        }
        return get_header_var(req, param.c_str());
    }

    static std::vector<multipart_t> get_multiparts(request_t req, const std::string& param = "")
    {
        std::vector<multipart_t> parts;
        auto&& type = get_header_var(req, "Content-Type");

        if (type.find("multipart/form-data") != std::string::npos) {
            size_t pos = 0;
            struct mg_http_part part;
            while ((pos = mg_http_next_multipart(req->body, pos, &part)) > 0) {
                multipart_t mp;
                mp.name = std::string(part.name.buf, part.name.len);
                mp.filename = std::string(part.filename.buf, part.filename.len);
                mp.data = part.body.buf;
                mp.len = part.body.len;
                parts.emplace_back(mp);
            }
        } else {
            if (!param.empty()) {
                multipart_t mp;
                mp.name = param;
                mp.filename = get_param(req, param);
                mp.data = req->body.buf;
                mp.len = req->body.len;
                parts.emplace_back(mp);
            }
        }

        return parts;
    }
};

#endif // _HTTP_INTERFACE_H_
