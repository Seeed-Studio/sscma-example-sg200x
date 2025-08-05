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

class http_interface {
public:
    http_interface() = default;
    virtual ~http_interface() = default;

protected:
    static inline const std::string STR_OK = "OK";
    static inline const std::string STR_FAILED = "Failed";

    template <typename T>
    static json to_json(T&& obj)
    {
        json j = json::object();
        try {
            j = json::parse(obj);
        } catch (const std::exception& e) {
            LOGV("%s", e.what());
        }
        return j;
    }

    static void response(response_t res, int code = 0,
        const std::string& msg = STR_OK, const json& data = json::object())
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
        std::string host = _get_header_var(req, "Host");
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
        std::string hdr = _get_header_var(req, "Host");
        size_t pos = hdr.find(':');
        return (pos != std::string::npos && pos < hdr.length() - 1)
            ? hdr.substr(pos + 1)
            : "";
    }

    static std::string get_param(request_t req, std::string param)
    {
        auto&& val = _get_http_var(req, param);
        if (!val.empty()) {
            return val;
        }
        return _get_header_var(req, param.c_str());
    }

    static std::string get_body_raw(request_t req)
    {
        return std::string(req->body.buf, req->body.len);
    }

    static json parse_body(request_t req)
    {
        std::string type = _get_header_var(req, "Content-Type");
        if (type.find("application/json") == std::string::npos) {
            return json({});
        }
        return to_json(get_body_raw(req));
    }

    typedef struct {
        std::string name;
        std::string filename;
        char* data;
        size_t len;
    } multipart_t;
    static std::vector<multipart_t> get_multiparts(request_t req, const std::string& param = "")
    {
        std::vector<multipart_t> parts;
        auto&& type = _get_header_var(req, "Content-Type");
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

    // token
    static constexpr uint32_t TOKEN_EXPIRATION_TIME = 3 * 60 * 60 * 24; // 3 days

    static void save_token(std::string& token)
    {
        _tokens[token] = time(nullptr);
        LOGV("save_token: %s, time: %ld", token.c_str(), _tokens[token]);
    }

    static bool check_token(std::string& token)
    {
        if (_tokens.find(token) == _tokens.end()) {
            LOGE("Not found token");
            return false;
        }
        if (_tokens[token] + TOKEN_EXPIRATION_TIME < time(nullptr)) {
            LOGV("Expired token");
            _tokens.erase(token);
            return false;
        }
        LOGV("Valid token");
        return true;
    }

private:
    static inline std::unordered_map<std::string, time_t> _tokens;

    static std::string _get_http_var(request_t req, std::string param)
    {
        struct mg_str v = mg_http_var(req->query, mg_str(param.c_str()));
        return std::string(v.buf, v.len);
    }

    static std::string _get_header_var(request_t req, const char* name)
    {
        mg_str* hdr = mg_http_get_header((mg_http_message*)req, name);
        return (hdr == nullptr) ? "" : std::string(hdr->buf, hdr->len);
    }
};

#endif // _HTTP_INTERFACE_H_
