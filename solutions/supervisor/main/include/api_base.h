#ifndef _API_BASE_H_
#define _API_BASE_H_

#include <array>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

#include "app_daemon.h"
#include "http_server.h"

#undef TAG
#define TAG "api_base"

#define _BASE_API_(auth_, method_, path_, sync_, async_, ctx_, state_) \
    apis.emplace_back(auth_, method_, path_, sync_, async_, ctx_, state_)

#define _SYNC_API_(auth_, method_, _group, _class, _func) _BASE_API_(auth_, #method_, \
    "/api/" _group "/" #_func,                                                        \
    std::bind(&_class::_func, this, std::placeholders::_1, std::placeholders::_2), nullptr, nullptr, nullptr)
#define SYNC_API(method_, _func) _SYNC_API_(true, method_, GROUP_NAME, CLASS_TYPE, _func)
#define SYNC_API_NOAUTH(method_, _func) _SYNC_API_(false, method_, GROUP_NAME, CLASS_TYPE, _func)

#define _CTX_API_(auth_, method_, _group, _class, _func) _BASE_API_(auth_, #method_, \
    "/api/" _group "/" #_func,                                                       \
    nullptr, nullptr, std::bind(&_class::_func, this, std::placeholders::_1), nullptr)
#define CTX_API(method_, _func) _CTX_API_(true, method_, GROUP_NAME, CLASS_TYPE, _func)
#define CTX_API_NOAUTH(method_, _func) _CTX_API_(false, method_, GROUP_NAME, CLASS_TYPE, _func)

class supervisor;

class api_base {
private:
    const std::string script_;

protected:
    const supervisor* sv_;
    std::vector<api_t> apis;
    virtual void register_apis() {};

public:
    api_base(const supervisor* sv = nullptr, const std::string& script = "")
        : sv_(sv)
        , script_(script)
    {
    }
    virtual ~api_base() = default;

    const std::vector<api_t>& get_apis() const { return apis; }

    std::string exec_shell_cmd(const std::string& cmd, const std::string& args = "")
    {
        std::array<char, 1024> buffer;
        std::string result;

        std::string full_cmd = script_ + " " + cmd + " " + args;
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(full_cmd.c_str(), "r"), pclose);
        MA_LOGD(TAG, "Executing: %s", full_cmd.c_str());
        if (!pipe) {
            MA_LOGE(TAG, "popen() failed: `%s`(%s)", full_cmd.c_str(), strerror(errno));
            throw std::runtime_error("popen() failed!");
        }

        while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
            result += buffer.data();
        }
        if (ferror(pipe.get())) {
            MA_LOGE(TAG, "fgets() encountered an I/O error: %s", strerror(errno));
            throw std::runtime_error("fgets() encountered an I/O error!");
        }

        if (!result.empty() && result.back() == '\n') {
            result.pop_back();
        }

        MA_LOGD(TAG, "Result: %s", result.c_str());
        return result;
    }

    std::string system_service(const std::string& service, const std::string& action)
    {
        MA_LOGD(TAG, "Service: %s %s", service.c_str(), action.c_str());
        return exec_shell_cmd("/etc/init.d/S*" + service, action);
    }

    std::string read_file(const std::string& path, const std::string& defaultname = "")
    {
        std::ifstream ifs(path);
        if (!ifs.is_open()) {
            return defaultname;
        }
        return std::string((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));
    }
};

#endif // _API_BASE_H_
