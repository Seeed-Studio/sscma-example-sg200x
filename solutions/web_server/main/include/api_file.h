#ifndef API_FILE_H
#define API_FILE_H

#include "api_base.h"

class api_file : public api_base {
private:
    static api_status_t deleteFile(const json& request, json& response)
    {
        response["code"] = 0;
        response["msg"] = "";
        response["data"] = json(
            "");

        return API_STATUS_OK;
    }

    static api_status_t queryFileList(const json& request, json& response)
    {
        response["code"] = 0;
        response["msg"] = "";
        response["data"] = json(
            "");

        return API_STATUS_OK;
    }

    static api_status_t uploadFile(const json& request, json& response)
    {
        response["code"] = 0;
        response["msg"] = "";
        response["data"] = json(
            "");

        return API_STATUS_OK;
    }

public:
    api_file()
        : api_base("fileMgr")
    {
        MA_LOGV("");

        REG_API(deleteFile);
        REG_API(queryFileList);
        REG_API(uploadFile);
    }

    ~api_file()
    {
        MA_LOGV("");
    }
};

#endif // API_FILE_H
