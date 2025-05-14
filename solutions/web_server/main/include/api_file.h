#include "api_base.h"

class api_file : public api_base {
public:
    api_file()
        : api_base("fileMgr")
    {
        printf("%s,%d\n", __func__, __LINE__);
        list.emplace_back(REST_API(file_api1));
    }

    ~api_file()
    {
        printf("%s,%d\n", __func__, __LINE__);
    }

    static void file_api1(mg_connection* c, mg_http_message* hm)
    {
        printf("%s,%d\n", __func__, __LINE__);
        mg_http_reply(c, 200, "Content-Type: text/plain\r\n", "%s\n", __func__);
    }
};