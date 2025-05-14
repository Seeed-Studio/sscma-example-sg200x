#ifndef API_BASE_H
#define API_BASE_H

#include <string>

#include "logger.hpp"
#include "mongoose.h"

using namespace std;

typedef void (*api_handler_t)(mg_connection* c, mg_http_message* hm);

class rest_api {
public:
    const bool _no_auth;
    const char* _uri;
    const api_handler_t _handler;

    rest_api(const char* uri, api_handler_t handler, bool no_auth = false)
        : _uri(uri)
        , _handler(handler)
        , _no_auth(no_auth)
    {
        printf("%s,%d: %s\n", __func__, __LINE__, _uri);
    }

    ~rest_api()
    {
        printf("%s,%d: %s\n", __func__, __LINE__, _uri);
    }
};

#define REST_API_FULL(__uri, __handler, __no_auth) \
    make_unique<rest_api>(__uri, __handler, __no_auth)
#define REST_API(__handler) \
    REST_API_FULL(#__handler, __handler, false)
#define REST_API_NO_AUTH(__handler) \
    REST_API_FULL(#__handler, __handler, true)

class api_base {
protected:
    const string _group;
    vector<unique_ptr<rest_api>> list;

public:
    api_base(string group = "")
        : _group(group)
    {
        printf("%s,%d: _group=%s\n", __func__, __LINE__, _group.c_str());
    }

    bool handler(mg_connection* c, mg_http_message* hm)
    {
        string uri = "/api/" + _group;
        int len = uri.length();
        if (0 != uri.compare(0, len, hm->uri.buf, 0, len)) {
            return false;
        }

        if (hm->uri.len > len) {
            if ('/' != hm->uri.buf[len]) {
                return false;
            }
            len++;
        }

        mg_str hm_uri;
        hm_uri.buf = hm->uri.buf + len;
        hm_uri.len = hm->uri.len - len;

        for (auto& li : list) {
            if (mg_match(hm_uri, mg_str(li->_uri), NULL)) {
                if (!li->_handler)
                    continue;
                li->_handler(c, hm);
                return true;
            }
        }

        return false;
    }
};

#endif // API_BASE_H