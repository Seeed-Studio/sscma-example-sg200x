#ifndef API_USER_H
#define API_USER_H

#include "api_base.h"

class api_user : public api_base {
private:

public:
    api_user()
        : api_base("userMgr")
    {
        printf("%s,%d\n", __func__, __LINE__);
    }

    ~api_user()
    {
        printf("%s,%d\n", __func__, __LINE__);
    }
};

#endif // API_USER_H
