#include "api_base.h"

class api_user : public api_base {
public:
    api_user()
        : api_base("userMgr")
    {
        printf("%s,%d\n", __func__, __LINE__);
        list.emplace_back(REST_API(user_api1));
    }

    ~api_user()
    {
        printf("%s,%d\n", __func__, __LINE__);
    }

    static void user_api1(mg_connection* c, mg_http_message* hm)
    {
        printf("%s,%d\n", __func__, __LINE__);
        mg_http_reply(c, 200, "Content-Type: text/plain\r\n", "%s\n", __func__);
    }
};