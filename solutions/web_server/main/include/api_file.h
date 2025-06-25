#ifndef API_FILE_H
#define API_FILE_H

#include "api_base.h"

class api_file : public api_base {
private:
    static api_status_t deleteFile(request_t req, response_t res)
    {
        std::string fname = get_param(req, "filePath");
        if (fname.empty()) {
            auto&& body = parse_body(req);
            if (body.contains("filePath")) {
                fname = body.value("filePath", "");
            }
        }
        const std::string& result = script(__func__, fname);
        response(res, (result == STR_OK) ? 0 : -1, result);
        return API_STATUS_OK;
    }

    static api_status_t queryFileList(request_t req, response_t res)
    {
        auto&& list = json::parse(script(__func__));
        LOGV("%s", list.dump(4).c_str());
        response(res, 0, STR_OK, list);
        return API_STATUS_OK;
    }

    static api_status_t uploadFile(request_t req, response_t res)
    {
        std::string dir = script(__func__);
        if (access(dir.c_str(), F_OK) != 0) {
            response(res, -1, "Directory is not accessible.");
            return API_STATUS_OK;
        }

        auto&& parts = get_multiparts(req);
        for (auto& part : parts) {
            if (part.filename.empty() || part.len == 0) {
                continue;
            }

            std::ofstream file(dir + "/" + part.filename, std::ios::binary);
            if (!file.is_open()) {
                continue;
            }
            file.write(part.data, part.len);
            file.close();
        }

        response(res);
        return API_STATUS_OK;
    }

public:
    api_file()
        : api_base("fileMgr")
    {
        LOGV("");
        REG_API(deleteFile);
        REG_API(queryFileList);
        REG_API(uploadFile);
    }

    ~api_file()
    {
        LOGV("");
    }
};

#endif // API_FILE_H
