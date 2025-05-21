#ifndef API_USER_H
#define API_USER_H

#include "api_base.h"


class api_user : public api_base {
private:
    static api_status_t addSShkey(request_t req, response_t res);
    static api_status_t deleteSShkey(request_t req, response_t res);
    static api_status_t login(request_t req, response_t res);
    static api_status_t queryUserInfo(request_t req, response_t res);
    static api_status_t setSShStatus(request_t req, response_t res);
    static api_status_t updatePassword(request_t req, response_t res);

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
