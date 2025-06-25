#ifndef API_BASE_H
#define API_BASE_H

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unistd.h>
#include <unordered_map>
#include <vector>

#include "json.hpp"
#include "logger.hpp"
#include "mongoose.h"
#include "version.h"

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

class rest_api {
public:
    rest_api(const api_handler_t handler, bool no_auth = false)
        : _handler(handler)
        , _no_auth(no_auth)
    {
    }
    ~rest_api() = default;

    api_status_t operator()(request_t req, response_t res) { return _handler(req, res); }

    inline bool no_auth() const { return _no_auth; }

private:
    const api_handler_t _handler;
    const bool _no_auth;
};

class http_interface {
public:
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

    typedef struct {
        std::string name;
        std::string filename;
        char* data;
        size_t len;
    } multipart_t;

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

#define REG_API_FULL(__uri, __handler, __no_auth) \
    register_api(__uri, __handler, __no_auth)
#define REG_API(__handler) \
    REG_API_FULL(#__handler, __handler, false)
#define REG_API_NO_AUTH(__handler) \
    REG_API_FULL(#__handler, __handler, true)

class api_base : public http_interface {
public:
    api_base(std::string group = "", std::string script = "")
        : _group(group)
    {
        if (_group.empty()) {
            _script = script + " ";
            LOGV("script: %s", _script.c_str());

            REG_API_NO_AUTH(version);
        }
    }

    void register_api(std::string uri, api_handler_t handler, bool no_auth = false)
    {
        _api_map[_group.empty() ? uri : _group + "/" + uri] = std::make_unique<rest_api>(handler, no_auth);
    }

    static api_status_t api_handler(request_t req, response_t res)
    {
        std::string uri = get_uri(req);
        auto pos = uri.find("/api/");
        if (pos == std::string::npos) {
            return API_STATUS_NEXT;
        }
        uri = uri.substr(pos + 5);

        auto api = _api_map.find(uri);
        if (api == _api_map.end()) {
            for (auto& [key, value] : _api_map) {
                if (uri.find(key) == 0) {
                    return (*value)(req, res);
                }
            }
            LOGE("API not implemented: %s", uri.c_str());
            return API_STATUS_NEXT;
        }
        if (!api->second->no_auth()) {
            std::string token = get_header_var(req, "Authorization");
            if (token.empty() || !check_token(token)) {
                LOGE("Unauthorized: %s", uri.c_str());
                return API_STATUS_UNAUTHORIZED;
            }
        }
        return (*api->second)(req, res);
    }

    template <typename... Args>
    static std::string script(const std::string& cmd, Args&&... args)
    {
        int timeout_sec = 30;
        std::stringstream ss;
        ((ss << std::forward<Args>(args) << " "), ...);
        std::string args_str = ss.str();
        if (!args_str.empty())
            args_str.pop_back();
        std::string full_cmd = _script + cmd + (args_str.empty() ? "" : " " + args_str);

        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(full_cmd.c_str(), "r"), pclose);
        LOGV("Executing: %s", full_cmd);

        if (!pipe) {
            LOGE("popen() failed: %s, errno=%d, strerror=%s", cmd, errno, strerror(errno));
            // throw std::runtime_error("popen() failed for: " + full_cmd);
            return "";
        }

        // non-blocking read
        int fd = fileno(pipe.get());
        int flags = fcntl(fd, F_GETFL, 0);
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);

        std::vector<char> buffer(512);
        std::string result = "";
        time_t start_time = time(nullptr);

        while (true) {
            if (time(nullptr) - start_time > timeout_sec) {
                LOGE("Command timeout after ", timeout_sec, " seconds: ", full_cmd);
                // throw std::runtime_error("Command execution timeout: " + full_cmd);
                break;
            }

            ssize_t bytes_read = read(fd, buffer.data(), buffer.size());
            if (bytes_read > 0) {
                result.append(buffer.data(), bytes_read);
            } else if (bytes_read == 0) {
                break; // EOF
            } else {
                if (errno != EAGAIN && errno != EWOULDBLOCK) {
                    LOGE("read() error: %s, errno=%d", strerror(errno), errno);
                    // throw std::runtime_error("read() error during command execution");
                    break;
                }
                usleep(1000); // 1ms
            }
        }

        // Get exit status
        int status = pclose(pipe.release());
        if (WIFEXITED(status)) {
            int exit_status = WEXITSTATUS(status);
            if (exit_status != 0) {
                LOGE("Command exited with status %d", exit_status);
            }
        } else {
            LOGE("Command terminated abnormally: %s, status=%d", full_cmd.c_str(), status);
        }

        // Strip trailing newlines
        if (!result.empty()) {
            size_t end = result.find_last_not_of("\r\n");
            if (end != std::string::npos) {
                result = result.substr(0, end + 1);
            } else {
                result.clear();
            }
        }

        LOGV("Completed: [%d] %s", result.size(), result.c_str());
        return result;
    }

protected:
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

    // utils
    static void string_split(std::string s, const char delimiter, std::vector<std::string>& output)
    {
        size_t start = 0;
        size_t end = s.find_first_of(delimiter);

        while (end <= std::string::npos) {
            output.emplace_back(s.substr(start, end - start));
            if (end == std::string::npos)
                break;
            start = end + 1;
            end = s.find_first_of(delimiter, start);
        }
    }

    static uint64_t uptime(void)
    {
        uint64_t uptime = 0;
        std::ifstream uptime_file("/proc/uptime");
        if (uptime_file.is_open()) {
            double uptime_seconds;
            uptime_file >> uptime_seconds;
            uptime = static_cast<uint64_t>(uptime_seconds * 1000);
            uptime_file.close();
        }
        return uptime;
    }

    static auto timestamp()
    {
        using namespace std::chrono;
        auto now = system_clock::now();
        auto timestamp = duration_cast<seconds>(now.time_since_epoch());
        return timestamp.count();
    }

private:
    static inline std::unordered_map<std::string, std::unique_ptr<rest_api>> _api_map;
    static inline std::unordered_map<std::string, time_t> _tokens;
    static inline std::string _script;
    const std::string _group;

    static api_status_t version(request_t req, response_t res)
    {
        res["uptime"] = uptime();
        res["timestamp"] = timestamp();
        response(res, 0, STR_OK, PROJECT_VERSION);
        return API_STATUS_OK;
    }
};

#endif // API_BASE_H
