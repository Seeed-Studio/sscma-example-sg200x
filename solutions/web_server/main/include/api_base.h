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

#include "mongoose.h"

using namespace std;
using json = nlohmann::json;

typedef enum {
    API_STATUS_OK = 0,
    API_STATUS_NEXT,
    API_STATUS_ERROR,
    API_STATUS_AUTHORIZED,
    API_STATUS_UNAUTHORIZED,
} api_status_t;

// typedef const json& request_t;
typedef const mg_http_message* request_t;
typedef json& response_t;

typedef api_status_t (*api_handler_t)(request_t req, response_t res);

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
        MA_LOGV(_uri);
    }

    ~rest_api()
    {
        MA_LOGV(_uri);
    }
};

typedef struct {
    string name;
    string filename;
    size_t len;
    char* buf;
} form_t;

class api_base {
protected:
    static inline const string STR_OK = "OK";
    static inline const json EMPTY_JSON = json::object();

public:
protected:
    vector<unique_ptr<rest_api>> list;

    void register_api(string uri, api_handler_t handler, bool no_auth = false)
    {
        list.emplace_back(make_unique<rest_api>(uri, handler, no_auth));
    }

    static string get_uri(request_t req)
    {
        return string(req->uri.buf, req->uri.len);
    }

    static string get_param(request_t req, string name)
    {
        struct mg_str v = mg_http_var(req->query, mg_str(name.c_str()));
        return (v.len == 0) ? "" : string(v.buf, v.len);
    }

    static string get_header_var(request_t req, const char* name)
    {
        mg_str* hdr = mg_http_get_header((mg_http_message*)req, name);
        return (hdr) ? string(hdr->buf, hdr->len) : "";
    }

    static bool is_path_valid(string& path)
    {
        return mg_path_is_sane(mg_str(path.c_str()));
    }

    static bool is_multipart(request_t req)
    {
        string type = get_header_var(req, "Content-Type");
        return (type.find("multipart/form-data") != string::npos);
    }

    static bool get_body(request_t req, json& body)
    {
        body = (req->body.len) ? json::parse(req->body.buf) : json();
        return body.empty()? false : true;
    }

    static bool get_form(request_t req, form_t& form, string name, string filename = "")
    {
        size_t pos = 0;
        struct mg_http_part part;

        while ((pos = mg_http_next_multipart(req->body, pos, &part)) > 0) {
            string _name = string(part.name.buf, part.name.len);
            string _fname = string(part.filename.buf, part.filename.len);

            if (_name == name) {
                if (!filename.empty() && _fname != filename) {
                    continue;
                }

                form.name = _name;
                form.filename = _fname;
                form.len = part.body.len;
                form.buf = part.body.buf;
                return true;
            }
        }
        return false;
    }

    static api_status_t save_file(request_t req, string param,
        string dir = "", string force_name = "")
    {
        string fname = get_param(req, param);
        char* data = req->body.buf;
        size_t len = req->body.len;

        if (is_multipart(req)) {
            form_t form;
            if (get_form(req, form, param)) {
                fname = form.filename;
                data = form.buf;
                len = form.len;
            }
        }

        if (!force_name.empty()) {
            fname = force_name;
        }
        if (fname.empty()) {
            return API_STATUS_ERROR;
        }

        string path = dir + fname;
        if (!is_path_valid(path)) {
            return API_STATUS_ERROR;
        }
        if (!write_file(path, data, len)) {
            return API_STATUS_ERROR;
        }

        return API_STATUS_OK;
    }

public:
    inline static string _script;
    const string _group;
    vector<unique_ptr<rest_api>>& get_list() { return list; }

    api_base(string group = "", string script = "")
        : _group(group)
    {
        if (_group.empty()) {
            _script = script + " ";
            MA_LOGV(_group, ", ", _script);

            register_api("version", [](request_t req, response_t res) {
                res["code"] = 0;
                res["msg"] = "";
                res["data"] = PROJECT_VERSION;
                res["uptime"] = 1234;
                res["timestamp"] = 9999;
                return API_STATUS_OK; }, true);
        }
    }

    static void response(response_t res, int code, 
        const string& msg = STR_OK, const json& data = EMPTY_JSON)
    {
        res["code"] = code;
        res["msg"] = msg;
        res["data"] = data;
    }

    static string genToken(void)
    {
        return script(__func__);
    }

    template<typename... Args>
    static string script(const string& cmd, Args&&... args)
    {
        int timeout_sec = 30;
        std::stringstream ss;
        ((ss << std::forward<Args>(args) << " "), ...);
        string args_str = ss.str();
        if (!args_str.empty()) args_str.pop_back();
        string full_cmd = _script + cmd + (args_str.empty() ? "" : " " + args_str);

        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(full_cmd.c_str(), "r"), pclose);
        MA_LOGV("Executing: ", full_cmd);

        if (!pipe) {
            MA_LOGE("popen() failed: ", full_cmd, "errno=", errno, ", strerror=", strerror(errno));
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
                MA_LOGE("Command timeout after ", timeout_sec, " seconds: ", full_cmd);
                throw std::runtime_error("Command execution timeout: " + full_cmd);
            }

            ssize_t bytes_read = read(fd, buffer.data(), buffer.size());
            if (bytes_read > 0) {
                result.append(buffer.data(), bytes_read);
            } else if (bytes_read == 0) {
                break; // EOF
            } else {
                if (errno != EAGAIN && errno != EWOULDBLOCK) {
                    MA_LOGE("read() error: ", strerror(errno), ", errno=", errno);
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
                MA_LOGE("Command exited with status ", exit_status);
            }
        } else {
            MA_LOGE("Command terminated abnormally: ", full_cmd);
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

    static bool delete_file(const string& path)
    {
        namespace fs = std::filesystem;
        try {
            fs::path file_path(path);
            if (!fs::exists(file_path)) {
                MA_LOGE("File not found: ", path);
                return false;
            }
            fs::remove(file_path);
            return true;
        } catch (const std::exception& e) {
            MA_LOGE("File delete error: ", path, " - ", e.what());
            return false;
        }
    }

    static bool get_folder(const string& path, vector<string>& files)
    {
        namespace fs = std::filesystem;
        try {
            fs::path dir_path(path);
            if (!fs::exists(dir_path) || !fs::is_directory(dir_path)) {
                MA_LOGE("Directory not found: ", path);
                return false;
            }
            for (const auto& entry : fs::directory_iterator(dir_path)) {
                files.push_back(entry.path().filename());
            }
            return true;
        } catch (const std::exception& e) {
            MA_LOGE("Directory read error: ", path, " - ", e.what());
            return false;
        }
    }

    static string read_file(const string& path, const string& default_str = "")
    {
        namespace fs = std::filesystem;
        try {
            fs::path file_path(path);

            if (!fs::exists(file_path)) {
                MA_LOGE("File not found: ", path);
                return default_str;
            }

            const auto file_size = fs::file_size(file_path);
            std::ifstream ifs(file_path, std::ios::binary);
            ifs.exceptions(std::ifstream::failbit | std::ifstream::badbit);

            std::string buffer;
            buffer.resize(file_size);

            ifs.read(&buffer[0], file_size);
            if (ifs.gcount() != static_cast<std::streamsize>(file_size)) {
                MA_LOGE("Partial read: ", path, " (expected:", file_size, "actual: ", ifs.gcount(), ")");
                return default_str;
            }

            return buffer;
        } catch (const std::exception& e) {
            MA_LOGE("File read error: ", path, " - ", e.what());
            return default_str;
        }
    }

    static bool write_file(const string& path, char* buf, size_t len)
    {
        namespace fs = std::filesystem;
        try {
            fs::path file_path(path);
            fs::create_directories(file_path.parent_path());
            std::ofstream ofs(file_path, std::ios::binary);
            ofs.exceptions(std::ofstream::failbit | std::ofstream::badbit);
            ofs.write(buf, len);
            ofs.close();
            return true;
        } catch (const std::exception& e) {
            MA_LOGE("File write error: ", path, " - ", e.what());
            return false;
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
