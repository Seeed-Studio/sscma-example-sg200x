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

using namespace std;
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
    static inline const string STR_OK = "OK";
    static inline const string STR_FAILED = "Failed";

    static void response(response_t res, int code = 0,
        const string& msg = STR_OK, const json& data = json({}))
    {
        res["code"] = code;
        res["msg"] = msg;
        res["data"] = data;
    }

    static string get_uri(request_t req)
    {
        return string(req->uri.buf, req->uri.len);
    }

    static string get_host(request_t req, bool ip_only = true)
    {
        string host = get_header_var(req, "Host");
        if (ip_only) {
            size_t pos = host.find(':');
            if (pos != string::npos) {
                host = host.substr(0, pos);
            }
        }
        return host;
    }

    static string get_port(request_t req)
    {
        string hdr = get_header_var(req, "Host");
        size_t pos = hdr.find(':');
        return (pos != string::npos && pos < hdr.length() - 1)
            ? hdr.substr(pos + 1)
            : "";
    }

    static string get_http_var(request_t req, string param)
    {
        struct mg_str v = mg_http_var(req->query, mg_str(param.c_str()));
        return string(v.buf, v.len);
    }

    static string get_header_var(request_t req, const char* name)
    {
        mg_str* hdr = mg_http_get_header((mg_http_message*)req, name);
        return (hdr == nullptr) ? "" : string(hdr->buf, hdr->len);
    }

    static json parse_body(request_t req)
    {
        string type = get_header_var(req, "Content-Type");
        if (type.find("application/json") == string::npos) {
            return json();
        }
        return (req->body.len == 0) ? json() : json::parse(req->body.buf, req->body.buf + req->body.len);
    }

    static string get_param(request_t req, string param)
    {
        auto&& val = get_http_var(req, param);
        if (!val.empty()) {
            return val;
        }
        return get_header_var(req, param.c_str());
    }

    typedef struct {
        string name;
        string filename;
        char* data;
        size_t len;
    } multipart_t;

    static vector<multipart_t> get_multiparts(request_t req, const string& param = "")
    {
        vector<multipart_t> parts;
        auto&& type = get_header_var(req, "Content-Type");

        if (type.find("multipart/form-data") != string::npos) {
            size_t pos = 0;
            struct mg_http_part part;
            while ((pos = mg_http_next_multipart(req->body, pos, &part)) > 0) {
                multipart_t mp;
                mp.name = string(part.name.buf, part.name.len);
                mp.filename = string(part.filename.buf, part.filename.len);
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
    api_base(string group = "", string script = "")
        : _group(group)
    {
        if (_group.empty()) {
            _script = script + " ";
            MA_LOGV(_group, ", ", _script);

            REG_API_NO_AUTH(version);
        }
    }

    void register_api(std::string uri, api_handler_t handler, bool no_auth = false)
    {
        _api_map[_group.empty() ? uri : _group + "/" + uri] = make_unique<rest_api>(handler, no_auth);
    }

    static api_status_t api_handler(request_t req, response_t res)
    {
        std::string uri = get_uri(req);
        auto pos = uri.find("/api/");
        if (pos == string::npos) {
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
            MA_LOGE("API not implemented: ", uri);
            return API_STATUS_NEXT;
        }
        if (!api->second->no_auth()) {
            string token = get_header_var(req, "Authorization");
            if (token.empty() || !check_token(token)) {
                MA_LOGE("Unauthorized: ", uri);
                return API_STATUS_UNAUTHORIZED;
            }
        }
        return (*api->second)(req, res);
    }

    template <typename... Args>
    static string script(const string& cmd, Args&&... args)
    {
        int timeout_sec = 30;
        std::stringstream ss;
        ((ss << std::forward<Args>(args) << " "), ...);
        string args_str = ss.str();
        if (!args_str.empty())
            args_str.pop_back();
        string full_cmd = _script + cmd + (args_str.empty() ? "" : " " + args_str);

        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(full_cmd.c_str(), "r"), pclose);
        MA_LOGV("Executing: ", full_cmd);

        if (!pipe) {
            MA_LOGE("popen() failed: ", cmd, "errno=", errno, ", strerror=", strerror(errno));
            throw std::runtime_error("popen() failed for: " + full_cmd);
        }

        // non-blocking read
        int fd = fileno(pipe.get());
        int flags = fcntl(fd, F_GETFL, 0);
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);

        std::vector<char> buffer(512);
        string result;
        time_t start_time = time(nullptr);

        while (true) {
            if (time(nullptr) - start_time > timeout_sec) {
                MA_LOGE("Command timeout after ", timeout_sec, " seconds: ", full_cmd);
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
                    MA_LOGE("read() error: ", strerror(errno), ", errno=", errno);
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
                MA_LOGE("Command exited with status ", exit_status);
            }
        } else {
            MA_LOGE("Command terminated abnormally: ", cmd);
        }

        // Strip trailing newlines
        if (!result.empty()) {
            size_t end = result.find_last_not_of("\r\n");
            if (end != string::npos) {
                result = result.substr(0, end + 1);
            } else {
                result.clear();
            }
        }

        MA_LOGV("Completed: [", result.size(), "] ", result);
        return result;
    }

protected:
    static constexpr uint32_t TOKEN_EXPIRATION_TIME = 3 * 60 * 60 * 24; // 3 days

    static void save_token(string& token)
    {
        _tokens[token] = time(nullptr);
        MA_LOGV("save_token: ", token, ", time: ", _tokens[token]);
    }

    static bool check_token(string& token)
    {
        if (_tokens.find(token) == _tokens.end()) {
            MA_LOGV("check_token: not found");
            return false;
        }
        if (_tokens[token] + TOKEN_EXPIRATION_TIME < time(nullptr)) {
            MA_LOGV("check_token: expired");
            _tokens.erase(token);
            return false;
        }
        MA_LOGV("check_token: ok");
        return true;
    }

    // utils
    static void string_split(string s, const char delimiter, vector<string>& output)
    {
        size_t start = 0;
        size_t end = s.find_first_of(delimiter);

        while (end <= string::npos) {
            output.emplace_back(s.substr(start, end - start));
            if (end == string::npos)
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
    static inline std::unordered_map<string, time_t> _tokens;
    static inline std::string _script;
    const string _group;

    static api_status_t version(request_t req, response_t res)
    {
        res["uptime"] = uptime();
        res["timestamp"] = timestamp();
        response(res, 0, STR_OK, PROJECT_VERSION);
        return API_STATUS_OK;
    }
};

#endif // API_BASE_H
