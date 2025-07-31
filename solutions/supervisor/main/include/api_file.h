#ifndef API_FILE_H
#define API_FILE_H

#include "api_base.h"

class api_file : public api_base {
private:
    static inline std::string _dir = "/userdata/app";

    static api_status_t deleteFile(request_t req, response_t res)
    {
        std::string fname = get_param(req, "filename");
        if (fname.empty()) {
            fname = parse_body(req).value("filename", "");
        }
        if (fname.empty()) {
            response(res, -1, "File path is empty.");
            return API_STATUS_OK;
        }
        if (std::remove((_dir + "/" + fname).c_str()) == 0) {
            response(res);
        } else {
            response(res, -1, "Delete file failed.");
        }

        return API_STATUS_OK;
    }

    static api_status_t queryFileList(request_t req, response_t res)
    {
        json data = json::object();
        for (const auto& entry : std::filesystem::directory_iterator(_dir)) {
            if (entry.is_regular_file()) {
                data["files"].push_back(entry.path().filename().string());
            } else if (entry.is_directory()) {
                data["dirs"].push_back(entry.path().filename().string());
            }
        }
        response(res, 0, STR_OK, data);
        return API_STATUS_OK;
    }

    static api_status_t uploadFile(request_t req, response_t res)
    {
        std::string dir = _dir;
        if (access(dir.c_str(), F_OK) != 0) {
            response(res, -1, "Directory is not accessible.");
            return API_STATUS_OK;
        }

        auto&& parts = get_multiparts(req, "filename");
        for (auto& part : parts) {
            if (part.filename.empty() || part.len == 0)
                continue;

            std::ofstream file(dir + "/" + part.filename, std::ios::binary);
            if (!file.is_open()) {
                response(res, -1, "File is not accessible.");
                return API_STATUS_OK;
            }
            file.write(part.data, part.len);
            file.close();
        }

        response(res);
        return API_STATUS_OK;
    }

    static api_status_t downloadFile(request_t req, response_t res)
    {
        std::string fname = get_param(req, "filename");
        if (fname.empty()) {
            fname = parse_body(req).value("filename", "");
        }
        if (fname.empty()) {
            response(res, -1, "File path is empty.");
            return API_STATUS_OK;
        }

        json data = json::object();
        data["file"] = _dir + "/" + fname;
        response(res, 0, STR_OK, data);
        return API_STATUS_REPLY_FILE;
    }

public:
    api_file()
        : api_base("fileMgr")
    {
        std::string dir = script(__func__);
        if (!dir.empty()) {
            _dir = dir;
        }
        LOGV("dir: %s", _dir.c_str());
        REG_API(deleteFile);
        REG_API(queryFileList);
        REG_API(uploadFile);
        REG_API(downloadFile);
    }

    ~api_file()
    {
        LOGV("");
    }
};

#endif // API_FILE_H
