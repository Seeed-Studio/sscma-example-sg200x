#ifndef API_FILE_H
#define API_FILE_H

#include "api_base.h"

class api_file : public api_base {
public:
    api_file()
        : api_base("fileMgr")
    {
        MA_LOGV("");
    }

    ~api_file()
    {
        MA_LOGV("");
    }
};

#endif // API_FILE_H
