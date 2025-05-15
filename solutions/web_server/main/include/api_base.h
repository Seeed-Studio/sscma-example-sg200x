#ifndef API_BASE_H
#define API_BASE_H

#include <string>

#include "logger.hpp"
#include "json.hpp"

using namespace std;
using json = nlohmann::json;

typedef enum {
    API_STATUS_OK = 0,
    API_STATUS_NEXT,
    API_STATUS_ERROR,
    API_STATUS_AUTHORIZED,
    API_STATUS_UNAUTHORIZED,
} api_status_t;

typedef api_status_t (*api_handler_t)(const json &request, json &response);
typedef bool (*api_match_t)(const string &req_uri, const string &api_uri);

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
        printf("%s,%d: %s\n", __func__, __LINE__, _uri.c_str());
    }

    ~rest_api()
    {
        printf("%s,%d: %s\n", __func__, __LINE__, _uri.c_str());
    }
};

class api_base {
protected:
    const string _group;
    vector<unique_ptr<rest_api>> list;

    void register_api(string uri, api_handler_t handler, bool no_auth = false)
    {
        list.emplace_back(make_unique<rest_api>(uri, handler, no_auth));
    }

public:
    api_base(string group = "")
        : _group(group)
    {
        printf("%s,%d: _group=%s\n", __func__, __LINE__, _group.c_str());
    }

    rest_api* get(string &req_uri, api_match_t match)
    {
        string uri = "/api/" + _group;
        int len = uri.length();

        if (req_uri.length() < len) {
            return nullptr;
        }
        if (0 != uri.compare(0, len, req_uri.c_str(), 0, len)) {
            return nullptr;
        }

        if (match) {
            for (auto& li : list) {
                string _uri = uri + "/" + li->_uri;
                if (match) {
                    if (match(req_uri, _uri)) {
                        return li.get();
                    }
                }
                else {
                    if (req_uri == _uri) {
                        return li.get();
                    }
                }
            }
        }

        return nullptr;
    }
};

#define REG_API_FULL(__uri, __handler, __no_auth) \
    register_api(__uri, __handler, __no_auth)
#define REG_API(__handler) \
    REG_API_FULL(#__handler, __handler, false)
#define REG_API_NO_AUTH(__handler) \
    REG_API_FULL(#__handler, __handler, true)

#endif // API_BASE_H
