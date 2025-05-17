#ifndef API_BASE_H
#define API_BASE_H

#include <array>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unistd.h>
#include <vector>

#include "json.hpp"
#include "logger.hpp"
#include "version.h"

using namespace std;
using json = nlohmann::json;

typedef enum {
    API_STATUS_OK = 0,
    API_STATUS_NEXT,
    API_STATUS_ERROR,
    API_STATUS_AUTHORIZED,
    API_STATUS_UNAUTHORIZED,
} api_status_t;

typedef api_status_t (*api_handler_t)(const json& request, json& response);

class rest_api {
public:
    const bool _no_auth;
    const string _uri;
    const api_handler_t _handler;

    rest_api(const string uri, api_handler_t handler, bool no_auth = false)
        : _uri(uri)
        , _handler(handler)
        , _no_auth(no_auth)
    {
        MA_LOGV("%s", _uri.c_str());
    }

    ~rest_api()
    {
        MA_LOGV("%s", _uri.c_str());
    }
};

class api_base {
protected:
    vector<unique_ptr<rest_api>> list;

    void register_api(string uri, api_handler_t handler, bool no_auth = false)
    {
        list.emplace_back(make_unique<rest_api>(uri, handler, no_auth));
    }

private:
    static constexpr char TAG[] = "api_base";

public:
    const string _group;
    vector<unique_ptr<rest_api>>& get_list() { return list; }

    api_base(string group = "")
        : _group(group)
    {
        MA_LOGV("%s", _group.c_str());

        if (_group.empty()) {
            register_api("version", [](const json& request, json& response) {
                response["code"] = 0;
                response["msg"] = "";
                response["data"] = PROJECT_VERSION;
                response["uptime"] = 1234;
                response["timestamp"] = 9999;
                return API_STATUS_OK; }, true);
        }
    }

    static string script(const string& cmd, const string& args = "", int timeout_sec = 30)
    {
        if (system(("which " + cmd + " > /dev/null 2>&1").c_str()) != 0) {
            MA_LOGE("Command not found: %s", cmd.c_str());
            throw std::runtime_error("Command not available: " + cmd);
        }

        string full_cmd = cmd;
        full_cmd += " " + args;
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(full_cmd.c_str(), "r"), pclose);
        MA_LOGD("Executing: %s", full_cmd.c_str());

        if (!pipe) {
            MA_LOGE("popen() failed: %s (errno=%d %s)",
                full_cmd.c_str(), errno, strerror(errno));
            throw std::runtime_error("popen() failed for: " + full_cmd);
        }

        // non-blocking read
        int fd = fileno(pipe.get());
        int flags = fcntl(fd, F_GETFL, 0);
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);

        std::vector<char> buffer(4096);
        string result;
        time_t start_time = time(nullptr);

        while (true) {
            if (time(nullptr) - start_time > timeout_sec) {
                MA_LOGE("Command timeout after %d seconds: %s",
                    timeout_sec, full_cmd.c_str());
                throw std::runtime_error("Command execution timeout: " + full_cmd);
            }

            ssize_t bytes_read = read(fd, buffer.data(), buffer.size());
            if (bytes_read > 0) {
                result.append(buffer.data(), bytes_read);
            } else if (bytes_read == 0) {
                break; // EOF
            } else {
                if (errno != EAGAIN && errno != EWOULDBLOCK) {
                    MA_LOGE("read() error: %s (errno=%d)",
                        strerror(errno), errno);
                    throw std::runtime_error("read() error during command execution");
                }
                usleep(100000); // 100ms
            }
        }

        // Get exit status
        int status = pclose(pipe.release());
        if (WIFEXITED(status)) {
            int exit_status = WEXITSTATUS(status);
            if (exit_status != 0) {
                MA_LOGE("Command exited with status %d: %s",
                    exit_status, full_cmd.c_str());
            }
        } else {
            MA_LOGE("Command terminated abnormally: %s", full_cmd.c_str());
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

        MA_LOGD("Execution completed. Output size: %zu bytes", result.size());
        return result;
    }

    static string read_file(const string& path, const string& default_str = "")
    {
        namespace fs = std::filesystem;
        try {
            fs::path file_path(path);

            if (!fs::exists(file_path)) {
                MA_LOGE("File not found: %s", path.c_str());
                return default_str;
            }

            const auto file_size = fs::file_size(file_path);
            std::ifstream ifs(file_path, std::ios::binary);
            ifs.exceptions(std::ifstream::failbit | std::ifstream::badbit);

            std::string buffer;
            buffer.resize(file_size);

            ifs.read(&buffer[0], file_size);
            if (ifs.gcount() != static_cast<std::streamsize>(file_size)) {
                MA_LOGE("Partial read: %s (expected:%zu actual:%d)",
                    path.c_str(), file_size, ifs.gcount());
                return default_str;
            }

            MA_LOGD("Successfully read %zu bytes from %s", file_size, path.c_str());
            return buffer;
        } catch (const std::exception& e) {
            MA_LOGE("File read error: %s - %s", path.c_str(), e.what());
            return default_str;
        }
    }
};

#define REG_API_FULL(__uri, __handler, __no_auth) \
    register_api(__uri, __handler, __no_auth)
#define REG_API(__handler) \
    REG_API_FULL(#__handler, __handler, false)
#define REG_API_NO_AUTH(__handler) \
    REG_API_FULL(#__handler, __handler, true)

#endif // API_BASE_H
