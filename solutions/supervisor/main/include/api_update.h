#ifndef _API_UPDATE_H_
#define _API_UPDATE_H_

#include "api_base.h"

#undef TAG
#define TAG "api_update"

class api_update : public api_base {
public:
    api_update(const std::string& path = "/usr/share/supervisor/scripts/upgrade.sh")
        : api_base(path)
    {
        register_apis();
    }

private:
    void register_apis() override {};

    // API_POST(updateSystem);
    // API_GET(getUpdateProgress);
    // API_POST(cancelUpdate);
};

#endif // _API_UPDATE_H_
