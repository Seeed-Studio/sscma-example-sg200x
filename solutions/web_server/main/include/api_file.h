#ifndef API_FILE_H
#define API_FILE_H

#include "api_base.h"

class api_file : public api_base {
public:
    api_file()
        : api_base("fileMgr")
    {
        printf("%s,%d\n", __func__, __LINE__);
    }

    ~api_file()
    {
        printf("%s,%d\n", __func__, __LINE__);
    }
};

#endif // API_FILE_H
