#ifndef _API_BASE_H_
#define _API_BASE_H_

#include <array>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

#include "app_daemon.h"
#include "http_server.h"

#undef TAG
#define TAG "api_base"

#define BASE_API(_type, _auth, _group, _class, _func) apis.emplace_back( \
    _type,                                                               \
    _auth,                                                               \
    "/api/" _group "/" #_func,                                           \
    std::bind(&_class::_func, this, std::placeholders::_1, std::placeholders::_2));

#define API_GET(_func) BASE_API(API_TYPE_GET, true, GROUP_NAME, CLASS_TYPE, _func)
#define API_GET_NOAUTH(_func) BASE_API(API_TYPE_GET, false, GROUP_NAME, CLASS_TYPE, _func)
#define API_POST(_func) BASE_API(API_TYPE_POST, true, GROUP_NAME, CLASS_TYPE, _func)
#define API_POST_NOAUTH(_func) BASE_API(API_TYPE_POST, false, GROUP_NAME, CLASS_TYPE, _func)

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

    std::string exec_shell_cmd(const std::string& cmd)
    {
        std::array<char, 1024> buffer;
        std::string result;

        std::string full_cmd = script_ + " " + cmd;
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
        std::string _cmd = "/etc/init.d/S*";
        _cmd += service;
        _cmd += " ";
        _cmd += action;

        MA_LOGD(TAG, "Service: %s", _cmd.c_str());
        return exec_shell_cmd(_cmd);
    }
};

#endif // _API_BASE_H_
