#ifndef API_FILE_H
#define API_FILE_H

#include "api_base.h"

class api_file : public api_base {
private:
    inline static string _def_dir;
    inline static string _param;

    static api_status_t deleteFile(request_t req, response_t res)
    {
        string fname = get_param(req, _param);

        if (delete_file(_def_dir + fname)) {
            response(res, 0, STR_OK);
        } else {
            response(res, -1, "Remove file failed.");
        }

        return API_STATUS_OK;
    }

    static api_status_t queryFileList(request_t req, response_t res)
    {
        std::vector<std::string> files;

        if (get_folder(_def_dir, files)) {
            response(res, 0, STR_OK, json(files));
        } else {
            response(res, -1, "Get file list failed.");
        }

        return API_STATUS_OK;
    }

    static api_status_t uploadFile(request_t req, response_t res)
    {
        MA_LOGV(_def_dir);
        if (API_STATUS_OK == save_file(req, _param, _def_dir)) {
            response(res, 0, STR_OK);
        } else {
            response(res, -1, "Upload file failed.");
        }
        res["data"] = json("");

        return API_STATUS_OK;
    }

public:
    api_file(string dir, string param = "filePath")
        : api_base("fileMgr")
    {
        _def_dir = dir;
        _param = param;
        MA_LOGV(_param, ", ", _def_dir);

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
