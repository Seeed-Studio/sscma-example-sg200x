#ifndef API_USER_H
#define API_USER_H

#include "api_base.h"

#define TOKEN_EXPIRATION_TIME 60 * 60 * 24 * 3 // 3 days

class api_user : public api_base {
private:
    static void gen_token(json& response);
    static api_status_t login(const json& request, json& response);
    static api_status_t queryUserInfo(const json& request, json& response);

public:
    api_user()
        : api_base("userMgr")
    {
        printf("%s,%d\n", __func__, __LINE__);
        REG_API_NO_AUTH(login);
        REG_API_NO_AUTH(queryUserInfo);
    }

    ~api_user()
    {
        printf("%s,%d\n", __func__, __LINE__);
    }
};

#endif // API_USER_H
