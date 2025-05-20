#ifndef API_FILE_H
#define API_FILE_H

#include "api_base.h"

class api_file : public api_base {
private:
    inline static string _param;
    inline static string _def_dir;

    static api_status_t deleteFile(request_t req, response_t res)
    {
        string fname = get_param(req, _param);

        if (delete_file(_def_dir + fname)) {
            res["code"] = 0;
            res["msg"] = "Remove file successfully.";
            res["data"] = json("");
        } else {
            res["code"] = -1;
            res["msg"] = "Remove file failed.";
            res["data"] = json("");
        }

        return API_STATUS_OK;
    }

    static api_status_t queryFileList(request_t req, response_t res)
    {
        std::vector<std::string> files;

        if (get_folder(_def_dir, files)) {
            res["code"] = 0;
            res["msg"] = "Get file list successfully.";
            res["data"]["fileList"] = json(files);
        } else {
            res["code"] = -1;
            res["msg"] = "Get file list failed";
            res["data"] = json("");
        }

        return API_STATUS_OK;
    }

    static api_status_t uploadFile(request_t req, response_t res)
    {
        MA_LOGV("%s", _def_dir.c_str());
        if (API_STATUS_OK == save_file(req, _param, _def_dir)) {
            res["code"] = 0;
            res["msg"] = "Upload file successfully.";
        } else {
            res["code"] = -1;
            res["msg"] = "Upload file failed.";
        }
        res["data"] = json("");

        return API_STATUS_OK;
    }

public:
    api_file(string param, string dir)
        : api_base("fileMgr")
    {
        _param = param;
        _def_dir = dir;
        MA_LOGV("%s, %s", _param.c_str(), _def_dir.c_str());

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
