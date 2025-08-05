#ifndef API_FILE_H
#define API_FILE_H

#include "api_base.h"
#include <algorithm>
#include <filesystem>
#include <fstream>

class api_file : public api_base {
private:
    static inline std::string _dir = "/userdata/app";

    // Helper function to get parameter from URL or body
    static std::string getParam(request_t req, const std::string& name) {
        try {
            std::string val = get_param(req, name);
            if (val.empty()) {
                auto params = parse_body(req);
                val         = params.value(name, "");
            }
            return val;
        } catch (const std::exception& e) {
            return "";
        }
    }

    static std::string decodePath(const std::string& path) {
        std::string decoded;
        for (size_t i = 0; i < path.length(); ++i) {
            if (path[i] == '%') {
                if (i + 2 < path.length()) {
                    int value;
                    std::stringstream ss;
                    ss << std::hex << path.substr(i + 1, 2);
                    ss >> value;
                    decoded += static_cast<char>(value);
                    i += 2;
                }
            } else {
                decoded += path[i];
            }
        }
        return decoded;
    }

    // Validate path to prevent directory traversal
    static bool isValidPath(const std::string& path) {
        try {
            if (path.empty())
                return true;  // Allow empty path (root)
            if (path == "." || path == "..")
                return false;
            if (path.find("../") != std::string::npos || path.find("..\\") != std::string::npos) {
                return false;
            }
            return true;
        } catch (const std::exception& e) {
            return false;
        }
    }

    // Validate filename
    static bool isValidFilename(const std::string& filename) {
        try {
            if (filename.empty())
                return false;
            if (filename == "." || filename == "..")
                return false;
            const std::string invalidChars = "<>:\"|?*\\";
            for (char c : invalidChars) {
                if (filename.find(c) != std::string::npos) {
                    return false;
                }
            }
            return true;
        } catch (const std::exception& e) {
            return false;
        }
    }

    // Construct full path
    static std::string getFullPath(const std::string& relativePath) {
        try {
            if (relativePath.empty()) {
                return _dir;
            }
            return _dir + "/" + relativePath;
        } catch (const std::exception& e) {
            return _dir;
        }
    }

    // List directory contents
    static api_status_t list(request_t req, response_t res) {
        try {
            std::string path = getParam(req, "path");

            path = decodePath(path);

            // Validate path
            if (!isValidPath(path)) {
                response(res, -1, "Invalid path.");
                return API_STATUS_OK;
            }

            std::string fullPath = getFullPath(path);
            

            // Check if path exists and is directory
            if (!std::filesystem::exists(fullPath)) {
                response(res, -1, "Path does not exist.");
                return API_STATUS_OK;
            }

            if (!std::filesystem::is_directory(fullPath)) {
                response(res, -1, "Path is not a directory.");
                return API_STATUS_OK;
            }

            json data           = json::object();
            data["path"]        = path.empty() ? "/" : path;
            data["directories"] = json::array();
            data["files"]       = json::array();

            try {
                for (const auto& entry : std::filesystem::directory_iterator(fullPath)) {
                    try {
                        json item        = json::object();
                        std::string name = entry.path().filename().string();
                        item["name"]     = name;

                        if (entry.is_directory()) {
                            item["type"] = "directory";
                            item["size"] = 0;
                            // Get modification time
                            auto ftime = std::filesystem::last_write_time(entry);
                            auto sctp  = std::chrono::time_point_cast<std::chrono::system_clock::duration>(ftime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
                            item["modified"] = std::chrono::duration_cast<std::chrono::seconds>(sctp.time_since_epoch()).count();
                            data["directories"].push_back(item);
                        } else if (entry.is_regular_file()) {
                            item["type"] = "file";
                            item["size"] = static_cast<uint64_t>(entry.file_size());
                            // Get modification time
                            auto ftime = std::filesystem::last_write_time(entry);
                            auto sctp  = std::chrono::time_point_cast<std::chrono::system_clock::duration>(ftime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
                            item["modified"] = std::chrono::duration_cast<std::chrono::seconds>(sctp.time_since_epoch()).count();
                            data["files"].push_back(item);
                        }
                    } catch (const std::exception& e) {
                        // Skip problematic entries
                        continue;
                    }
                }

                response(res, 0, "Success", data);
            } catch (const std::exception& e) {
                response(res, -1, "Failed to read directory: " + std::string(e.what()));
                return API_STATUS_OK;
            }

        } catch (const std::exception& e) {
            response(res, -1, "Internal server error: " + std::string(e.what()));
        } catch (...) {
            response(res, -1, "Unknown error occurred.");
        }

        return API_STATUS_OK;
    }

    // Create directory
    static api_status_t mkdir(request_t req, response_t res) {
        try {
            std::string path = getParam(req, "path");

            path = decodePath(path);

            if (path.empty()) {
                response(res, -1, "Path is empty.");
                return API_STATUS_OK;
            }

            if (!isValidPath(path)) {
                response(res, -1, "Invalid path.");
                return API_STATUS_OK;
            }

            std::string fullPath = getFullPath(path);

            // Check if already exists
            if (std::filesystem::exists(fullPath)) {
                response(res, -1, "Path already exists.");
                return API_STATUS_OK;
            }

            // Create directory
            if (std::filesystem::create_directories(fullPath)) {
                json data    = json::object();
                data["path"] = path;
                response(res, 0, "Directory created.", data);
            } else {
                response(res, -1, "Failed to create directory.");
            }

        } catch (const std::filesystem::filesystem_error& e) {
            response(res, -1, "Filesystem error: " + std::string(e.what()));
        } catch (const std::exception& e) {
            response(res, -1, "Internal server error: " + std::string(e.what()));
        } catch (...) {
            response(res, -1, "Unknown error occurred.");
        }

        return API_STATUS_OK;
    }

    // Delete file or directory
    static api_status_t remove(request_t req, response_t res) {
        try {
            std::string path = getParam(req, "path");

            path = decodePath(path);

            if (path.empty()) {
                response(res, -1, "Path is empty.");
                return API_STATUS_OK;
            }

            if (!isValidPath(path)) {
                response(res, -1, "Invalid path.");
                return API_STATUS_OK;
            }

            std::string fullPath = getFullPath(path);
            LOGV("fullPath: %s", fullPath.c_str());
            // Check if exists
            if (!std::filesystem::exists(fullPath)) {
                response(res, -1, "Path does not exist.");
                return API_STATUS_OK;
            }

            try {
                if (std::filesystem::is_directory(fullPath)) {
                    // For directory, check if empty
                    if (!std::filesystem::is_empty(fullPath)) {
                        response(res, -1, "Directory is not empty.");
                        return API_STATUS_OK;
                    }
                    if (std::filesystem::remove(fullPath)) {
                        json data    = json::object();
                        data["path"] = path;
                        data["type"] = "directory";
                        response(res, 0, "Directory removed.", data);
                    } else {
                        response(res, -1, "Failed to remove directory.");
                    }
                } else {
                    if (std::filesystem::remove(fullPath)) {
                        json data    = json::object();
                        data["path"] = path;
                        data["type"] = "file";
                        response(res, 0, "File removed.", data);
                    } else {
                        response(res, -1, "Failed to remove file.");
                    }
                }
            } catch (const std::filesystem::filesystem_error& e) {
                response(res, -1, "Filesystem error: " + std::string(e.what()));
            }

        } catch (const std::exception& e) {
            response(res, -1, "Internal server error: " + std::string(e.what()));
        } catch (...) {
            response(res, -1, "Unknown error occurred.");
        }

        return API_STATUS_OK;
    }

    // Upload file
    static api_status_t upload(request_t req, response_t res) {
        try {
            std::string path = getParam(req, "path");

            path = decodePath(path);

            LOGV("path: %s", path.c_str());

            // Validate path
            if (!path.empty() && !isValidPath(path)) {
                response(res, -1, "Invalid path.");
                return API_STATUS_OK;
            }

            std::string uploadDir = getFullPath(path);

            // Check if directory exists
            if (!path.empty() && !std::filesystem::exists(uploadDir)) {
                response(res, -1, "Upload directory does not exist.");
                return API_STATUS_OK;
            }

            if (!path.empty() && !std::filesystem::is_directory(uploadDir)) {
                response(res, -1, "Upload path is not a directory.");
                return API_STATUS_OK;
            }

            // Get uploaded files
            auto&& parts     = get_multiparts(req, "file");
            json results     = json::array();
            int successCount = 0;

            for (auto& part : parts) {
                try {
                    if (part.filename.empty() || part.len == 0)
                        continue;

                    if (!isValidFilename(part.filename)) {
                        json result = {{"filename", part.filename}, {"status", "failed"}, {"message", "Invalid filename"}};
                        results.push_back(result);
                        continue;
                    }

                    std::string fullPath = uploadDir + "/" + part.filename;

                    std::ofstream file(fullPath, std::ios::binary);
                    if (!file.is_open()) {
                        json result = {{"filename", part.filename}, {"status", "failed"}, {"message", "Cannot create file"}};
                        results.push_back(result);
                        continue;
                    }

                    file.write(part.data, part.len);
                    file.close();

                    json result = {{"filename", part.filename}, {"status", "success"}, {"size", part.len}};
                    results.push_back(result);
                    successCount++;
                } catch (const std::exception& e) {
                    json result = {{"filename", part.filename}, {"status", "failed"}, {"message", "Upload error: " + std::string(e.what())}};
                    results.push_back(result);
                }
            }

            json data       = json::object();
            data["uploads"] = results;
            data["count"]   = successCount;

            if (successCount > 0) {
                response(res, 0, "Upload completed.", data);
            } else {
                response(res, -1, "Upload failed.", data);
            }

        } catch (const std::exception& e) {
            response(res, -1, "Internal server error: " + std::string(e.what()));
        } catch (...) {
            response(res, -1, "Unknown error occurred.");
        }

        return API_STATUS_OK;
    }

    // Download file
    static api_status_t download(request_t req, response_t res) {
        try {
            std::string path = getParam(req, "path");

            path = decodePath(path);

            if (path.empty()) {
                response(res, -1, "Path is empty.");
                return API_STATUS_OK;
            }

            if (!isValidPath(path)) {
                response(res, -1, "Invalid path.");
                return API_STATUS_OK;
            }

            std::string fullPath = getFullPath(path);

            // Check if file exists
            if (!std::filesystem::exists(fullPath)) {
                response(res, -1, "File does not exist.");
                return API_STATUS_OK;
            }

            if (!std::filesystem::is_regular_file(fullPath)) {
                response(res, -1, "Path is not a file.");
                return API_STATUS_OK;
            }

            json data    = json::object();
            data["file"] = fullPath;
            response(res, 0, "File ready.", data);
            return API_STATUS_REPLY_FILE;

        } catch (const std::filesystem::filesystem_error& e) {
            response(res, -1, "Filesystem error: " + std::string(e.what()));
        } catch (const std::exception& e) {
            response(res, -1, "Internal server error: " + std::string(e.what()));
        } catch (...) {
            response(res, -1, "Unknown error occurred.");
        }

        return API_STATUS_OK;
    }

    // Rename file or directory
    static api_status_t rename(request_t req, response_t res) {
        try {
            std::string oldPath = getParam(req, "old_path");
            std::string newPath = getParam(req, "new_path");

            if (oldPath.empty() || newPath.empty()) {
                response(res, -1, "Paths are empty.");
                return API_STATUS_OK;
            }

            if (!isValidPath(oldPath) || !isValidPath(newPath)) {
                response(res, -1, "Invalid paths.");
                return API_STATUS_OK;
            }

            std::string fullOldPath = getFullPath(oldPath);
            std::string fullNewPath = getFullPath(newPath);

            // Check if source exists
            if (!std::filesystem::exists(fullOldPath)) {
                response(res, -1, "Source does not exist.");
                return API_STATUS_OK;
            }

            // Check if destination already exists
            if (std::filesystem::exists(fullNewPath)) {
                response(res, -1, "Destination already exists.");
                return API_STATUS_OK;
            }

            // Rename
            std::filesystem::rename(fullOldPath, fullNewPath);
            json data        = json::object();
            data["old_path"] = oldPath;
            data["new_path"] = newPath;
            response(res, 0, "Renamed successfully.", data);


        }catch (const std::filesystem::filesystem_error& e) {
            response(res, -1, "Filesystem error: " + std::string(e.what()));
        } catch (const std::exception& e) {
            response(res, -1, "Internal server error: " + std::string(e.what()));
        } catch (...) {
            response(res, -1, "Unknown error occurred.");
        }

        return API_STATUS_OK;
    }

    // Get file/directory info
    static api_status_t info(request_t req, response_t res) {
        try {
            std::string path = getParam(req, "path");

            path = decodePath(path);

            if (path.empty()) {
                response(res, -1, "Path is empty.");
                return API_STATUS_OK;
            }

            if (!isValidPath(path)) {
                response(res, -1, "Invalid path.");
                return API_STATUS_OK;
            }

            std::string fullPath = getFullPath(path);

            // Check if exists
            if (!std::filesystem::exists(fullPath)) {
                response(res, -1, "Path does not exist.");
                return API_STATUS_OK;
            }

            json data            = json::object();
            data["path"]         = path;
            data["is_directory"] = std::filesystem::is_directory(fullPath);
            data["is_file"]      = std::filesystem::is_regular_file(fullPath);

            if (std::filesystem::is_regular_file(fullPath)) {
                data["size"] = static_cast<uint64_t>(std::filesystem::file_size(fullPath));
            } else {
                data["size"] = 0;
            }

            // Get modification time
            auto ftime       = std::filesystem::last_write_time(fullPath);
            auto sctp        = std::chrono::time_point_cast<std::chrono::system_clock::duration>(ftime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
            data["modified"] = std::chrono::duration_cast<std::chrono::seconds>(sctp.time_since_epoch()).count();

            response(res, 0, "Info retrieved.", data);

        } catch (const std::filesystem::filesystem_error& e) {
            response(res, -1, "Filesystem error: " + std::string(e.what()));
        } catch (const std::exception& e) {
            response(res, -1, "Internal server error: " + std::string(e.what()));
        } catch (...) {
            response(res, -1, "Unknown error occurred.");
        }

        return API_STATUS_OK;
    }

public:
    api_file() : api_base("fileMgr") {
        try {
            std::string dir = script(__func__);
            if (!dir.empty()) {
                _dir = dir;
            }

            // Register API endpoints
            REG_API(list);      // GET  /api/file/list
            REG_API(mkdir);     // POST /api/file/mkdir
            REG_API(remove);    // POST /api/file/remove
            REG_API(upload);    // POST /api/file/upload
            REG_API(download);  // GET  /api/file/download
            REG_API(rename);    // POST /api/file/rename
            REG_API(info);      // GET  /api/file/info
        } catch (const std::exception& e) {
            // Handle constructor errors
        }
    }

    ~api_file() {}
};

#endif  // API_FILE_H