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

#include "logger.hpp"

#include "http_interface.h"
#include "version.h"

#define REG_API_FULL(__uri, __handler, __no_auth) \
    register_api(__uri, __handler, __no_auth)
#define REG_API(__handler) \
    REG_API_FULL(#__handler, __handler, false)
#define REG_API_NO_AUTH(__handler) \
    REG_API_FULL(#__handler, __handler, true)

class rest_api {
public:
    rest_api(const api_handler_t handler, bool no_auth = false)
        : _handler(handler)
        , _no_auth(no_auth)
    {
    }

    virtual ~rest_api() = default;

    api_status_t operator()(request_t req, response_t res) { return _handler(req, res); }

    inline bool no_auth() const { return _no_auth; }

private:
    const api_handler_t _handler;
    const bool _no_auth;
};

class api_base : public http_interface {
public:
    api_base(std::string group = "")
        : _group(group)
    {
        if (_group.empty()) {
            REG_API_NO_AUTH(version);
        }
    }

    virtual ~api_base() = default;

    static void set_force_no_auth(bool no_auth) { _force_no_auth = no_auth; }
    static void set_script(std::string dir) { _script = dir; }

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
        if (!_force_no_auth && !api->second->no_auth()) {
            std::string token = get_param(req, "Authorization");
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
        ((ss << "'" << std::forward<Args>(args) << "' "), ...);
        std::string args_str = ss.str();
        std::string full_cmd = _script + " " + cmd + " " + args_str;

        std::unique_ptr<FILE, decltype(&pclose)>
            pipe(popen(full_cmd.c_str(), "r"), pclose);
        LOGV("Executing: %s", full_cmd.c_str());

        if (!pipe) {
            LOGE("popen() failed: %s, errno=%d, strerror=%s",
                full_cmd.c_str(), errno, strerror(errno));
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
                LOGE("Command timeout after %d seconds: %s", timeout_sec, full_cmd.c_str());
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
            // int exit_status = WEXITSTATUS(status);
            // if (exit_status != 0) {
            //     LOGE("Command exited with status %d", exit_status);
            // }
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

    template <typename T>
    static json parse_result(T&& result) { return to_json(result); }

    using vvstr_t = std::vector<std::vector<std::string>>;
    static vvstr_t parse_result(std::string file, char delimiter, bool skip_header = false)
    {
        vvstr_t result;
        std::ifstream f(file);
        if (!f.is_open()) {
            LOGE("Failed to open file: %s", file.c_str());
            return result;
        }

        std::string line;
        if (skip_header)
            std::getline(f, line);
        while (std::getline(f, line)) {
            if (line.empty())
                continue;
            std::stringstream ss(std::move(line));
            std::string field;
            std::vector<std::string> fields;
            while (std::getline(ss, field, delimiter)) {
                fields.push_back(field);
            }
            if (fields.size() < 1)
                continue;
            result.push_back(fields);
        }
        return result;
    }

    // utils
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
    const std::string _group;

    static inline bool _force_no_auth = false;
    static inline std::string _script;
    static inline std::unordered_map<std::string, std::unique_ptr<rest_api>> _api_map;

    static api_status_t version(request_t req, response_t res)
    {
        res["uptime"] = uptime();
        res["timestamp"] = timestamp();
        response(res, 0, STR_OK, PROJECT_VERSION);
        return API_STATUS_OK;
    }
};
#endif // API_BASE_H
