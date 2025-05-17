#ifndef API_USER_H
#define API_USER_H

#include "api_base.h"

#define TOKEN_EXPIRATION_TIME 60 * 60 * 24 * 3 // 3 days

class api_user : public api_base {
private:
    static api_status_t addSShkey(const json& request, json& response);
    static api_status_t deleteSShkey(const json& request, json& response);
    static api_status_t login(const json& request, json& response);
    static api_status_t queryUserInfo(const json& request, json& response);
    static api_status_t setSShStatus(const json& request, json& response);
    static api_status_t updatePassword(const json& request, json& response);

public:
    api_user()
        : api_base("userMgr")
    {
        MA_LOGV("");

        REG_API(addSShkey);
        REG_API(deleteSShkey);
        REG_API_NO_AUTH(login);
        REG_API(queryUserInfo); // fixed: no auth
        REG_API(setSShStatus); // fixed: no auth
        REG_API(updatePassword); // fixed: no auth
    }

    ~api_user()
    {
        MA_LOGV("");
    }
};

#endif // API_USER_H
