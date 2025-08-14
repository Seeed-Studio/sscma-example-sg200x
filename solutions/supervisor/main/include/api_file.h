#ifndef API_FILE_H
#define API_FILE_H

#include "api_base.h"
#include <algorithm>
#include <filesystem>
#include <fstream>

class api_file : public api_base {
private:
    static inline std::string _local_dir = "/userdata";
    static inline std::string _sd_dir    = "/mnt/sd";

    // Check if SD card is available
    static bool isSDAvailable() {
        try {
            return std::filesystem::exists(_sd_dir) && std::filesystem::is_directory(_sd_dir);
        } catch (...) {
            return false;
        }
    }

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

    // Validate storage parameter
    static bool isValidStorage(const std::string& storage) {
        return storage.empty() || storage == "local" || storage == "sd";
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

    // Validate path to prevent directory traversal and hidden files/directories
    static bool isValidPath(const std::string& path) {
        try {
            if (path.empty())
                return true;  // Allow empty path (root)
            if (path == "." || path == "..")
                return false;
            if (path.find("../") != std::string::npos || path.find("..\\") != std::string::npos) {
                return false;
            }
            // Check for hidden files or directories (starting with '.')
            std::string cleanPath = path;
            if (cleanPath[0] == '/')
                cleanPath = cleanPath.substr(1);  // Remove leading slash for consistency
            std::istringstream pathStream(cleanPath);
            std::string component;
            while (std::getline(pathStream, component, '/')) {
                if (!component.empty() && component[0] == '.') {
                    return false;  // Reject any component starting with a dot
                }
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

    // Construct full path based on storage type
    static std::string getFullPath(const std::string& relativePath, const std::string& storage) {
        try {
            std::string baseDir = (storage == "sd") ? _sd_dir : _local_dir;
            if (relativePath.empty()) {
                return baseDir;
            }
            return baseDir + "/" + relativePath;
        } catch (const std::exception& e) {
            return (storage == "sd") ? _sd_dir : _local_dir;
        }
    }

    // List directory contents
    static api_status_t list(request_t req, response_t res) {
        try {
            std::string path    = getParam(req, "path");
            std::string storage = getParam(req, "storage");
            path                = decodePath(path);

            // Default to local if storage is empty
            std::string effectiveStorage = storage.empty() ? "local" : storage;

            // Validate storage
            if (!isValidStorage(effectiveStorage)) {
                response(res, -1, "Invalid storage parameter. Use 'local' or 'sd'.");
                return API_STATUS_OK;
            }
            if (effectiveStorage == "sd" && !isSDAvailable()) {
                response(res, -1, "SD card not available.");
                return API_STATUS_OK;
            }

            // Validate path
            if (!isValidPath(path)) {
                response(res, -1, "Invalid path.");
                return API_STATUS_OK;
            }

            std::string fullPath = getFullPath(path, effectiveStorage);

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
            data["storage"]     = effectiveStorage;
            data["directories"] = json::array();
            data["files"]       = json::array();

            // Add storage info for root path
            if (path.empty() || path == "/") {
                try {
                    std::filesystem::space_info space = std::filesystem::space(fullPath);
                    uint64_t totalCapacity            = space.capacity;
                    uint64_t freeSpace                = space.free;
                    uint64_t usedSpace                = totalCapacity - freeSpace;
                    data["total"]                     = totalCapacity;
                    data["used"]                      = usedSpace;
                    data["free"]                      = freeSpace;
                } catch (const std::exception& e) {
                    // Log error but don't fail the entire request
                    // Optionally, you could add an error field to the response
                }
            }

            try {
                for (const auto& entry : std::filesystem::directory_iterator(fullPath)) {
                    try {
                        json item        = json::object();
                        std::string name = entry.path().filename().string();
                        // don't show hidden file & directory
                        if (!name.empty() && name[0] == '.') {
                            continue;
                        }
                        item["name"] = name;

                        if (entry.is_directory()) {
                            item["type"] = "directory";
                            item["size"] = 0;
                            auto ftime   = std::filesystem::last_write_time(entry);
                            auto sctp    = std::chrono::time_point_cast<std::chrono::system_clock::duration>(ftime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
                            item["modified"] = std::chrono::duration_cast<std::chrono::seconds>(sctp.time_since_epoch()).count();
                            data["directories"].push_back(item);
                        } else if (entry.is_regular_file()) {
                            item["type"] = "file";
                            item["size"] = static_cast<uint64_t>(entry.file_size());
                            auto ftime   = std::filesystem::last_write_time(entry);
                            auto sctp    = std::chrono::time_point_cast<std::chrono::system_clock::duration>(ftime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
                            item["modified"] = std::chrono::duration_cast<std::chrono::seconds>(sctp.time_since_epoch()).count();
                            data["files"].push_back(item);
                        }
                    } catch (const std::exception& e) {
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
            std::string path    = getParam(req, "path");
            std::string storage = getParam(req, "storage");
            path                = decodePath(path);

            // Default to local if storage is empty
            std::string effectiveStorage = storage.empty() ? "local" : storage;

            if (path.empty()) {
                response(res, -1, "Path is empty.");
                return API_STATUS_OK;
            }

            if (!isValidStorage(effectiveStorage)) {
                response(res, -1, "Invalid storage parameter. Use 'local' or 'sd'.");
                return API_STATUS_OK;
            }
            if (effectiveStorage == "sd" && !isSDAvailable()) {
                response(res, -1, "SD card not available.");
                return API_STATUS_OK;
            }

            if (!isValidPath(path)) {
                response(res, -1, "Invalid path.");
                return API_STATUS_OK;
            }

            std::string fullPath = getFullPath(path, effectiveStorage);

            if (std::filesystem::exists(fullPath)) {
                response(res, -1, "Path already exists.");
                return API_STATUS_OK;
            }

            if (std::filesystem::create_directories(fullPath)) {
                json data       = json::object();
                data["path"]    = path;
                data["storage"] = effectiveStorage;
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
            std::string path    = getParam(req, "path");
            std::string storage = getParam(req, "storage");
            path                = decodePath(path);

            // Default to local if storage is empty
            std::string effectiveStorage = storage.empty() ? "local" : storage;

            if (path.empty()) {
                response(res, -1, "Path is empty.");
                return API_STATUS_OK;
            }

            if (!isValidStorage(effectiveStorage)) {
                response(res, -1, "Invalid storage parameter. Use 'local' or 'sd'.");
                return API_STATUS_OK;
            }
            if (effectiveStorage == "sd" && !isSDAvailable()) {
                response(res, -1, "SD card not available.");
                return API_STATUS_OK;
            }

            if (!isValidPath(path)) {
                response(res, -1, "Invalid path.");
                return API_STATUS_OK;
            }

            std::string fullPath = getFullPath(path, effectiveStorage);
            LOGV("fullPath: %s", fullPath.c_str());

            if (!std::filesystem::exists(fullPath)) {
                response(res, -1, "Path does not exist.");
                return API_STATUS_OK;
            }

            try {
                if (std::filesystem::is_directory(fullPath)) {
                    if (!std::filesystem::is_empty(fullPath)) {
                        response(res, -1, "Directory is not empty.");
                        return API_STATUS_OK;
                    }
                    if (std::filesystem::remove(fullPath)) {
                        json data       = json::object();
                        data["path"]    = path;
                        data["type"]    = "directory";
                        data["storage"] = effectiveStorage;
                        response(res, 0, "Directory removed.", data);
                    } else {
                        response(res, -1, "Failed to remove directory.");
                    }
                } else {
                    if (std::filesystem::remove(fullPath)) {
                        json data       = json::object();
                        data["path"]    = path;
                        data["type"]    = "file";
                        data["storage"] = effectiveStorage;
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
            std::string path      = getParam(req, "path");
            std::string storage   = getParam(req, "storage");
            std::string offsetStr = getParam(req, "offset");  // Get offset parameter as string
            path                  = decodePath(path);

            // Default to local if storage is not specified
            std::string effectiveStorage = storage.empty() ? "local" : storage;

            LOGV("path: %s", path.c_str());

            // Validate storage type
            if (!isValidStorage(effectiveStorage)) {
                response(res, -1, "Invalid storage parameter. Use 'local' or 'sd'.");
                return API_STATUS_OK;
            }

            // Check if SD card is available when using 'sd' storage
            if (effectiveStorage == "sd" && !isSDAvailable()) {
                response(res, -1, "SD card not available.");
                return API_STATUS_OK;
            }

            // Validate path format
            if (!path.empty() && !isValidPath(path)) {
                response(res, -1, "Invalid path.");
                return API_STATUS_OK;
            }

            // Get the full upload directory path
            std::string uploadDir = getFullPath(path, effectiveStorage);

            // Check if upload directory exists
            if (!path.empty() && !std::filesystem::exists(uploadDir)) {
                response(res, -1, "Upload directory does not exist.");
                return API_STATUS_OK;
            }

            // Ensure the path is a directory
            if (!path.empty() && !std::filesystem::is_directory(uploadDir)) {
                response(res, -1, "Upload path is not a directory.");
                return API_STATUS_OK;
            }

            // Parse offset (default to 0 if not provided)
            size_t offset = 0;
            if (!offsetStr.empty()) {
                try {
                    offset = std::stoull(offsetStr);  // Support large file offsets
                } catch (...) {
                    response(res, -1, "Invalid offset parameter. Must be a non-negative integer.");
                    return API_STATUS_OK;
                }
            }

            // Get uploaded file parts
            auto&& parts     = get_multiparts(req, "file");
            json results     = json::array();
            int successCount = 0;

            for (auto& part : parts) {
                try {
                    // Skip empty or invalid parts
                    if (part.filename.empty() || part.len == 0)
                        continue;

                    // Validate filename
                    if (!isValidFilename(part.filename)) {
                        json result = {{"filename", part.filename}, {"status", "failed"}, {"message", "Invalid filename"}};
                        results.push_back(result);
                        continue;
                    }

                    // Full path to the target file
                    std::string fullPath = uploadDir + "/" + part.filename;

                    // Ensure parent directory exists
                    std::filesystem::path parentDir = std::filesystem::path(fullPath).parent_path();
                    if (!std::filesystem::exists(parentDir)) {
                        std::filesystem::create_directories(parentDir);
                    }

                    // Open file in read/write binary mode to support random access
                    std::fstream file(fullPath, std::ios::in | std::ios::out | std::ios::binary);
                    bool fileExisted = file.is_open();

                    if (!fileExisted) {
                        // If file doesn't exist, create it
                        file.open(fullPath, std::ios::out | std::ios::binary);
                        if (!file.is_open()) {
                            json result = {{"filename", part.filename}, {"status", "failed"}, {"message", "Cannot create file"}};
                            results.push_back(result);
                            continue;
                        }
                        // Close and reopen in read/write mode for seeking
                        file.close();
                        file.open(fullPath, std::ios::in | std::ios::out | std::ios::binary);
                        if (!file.is_open()) {
                            json result = {{"filename", part.filename}, {"status", "failed"}, {"message", "Cannot reopen file"}};
                            results.push_back(result);
                            continue;
                        }
                    }

                    // Seek to the specified offset
                    file.seekp(offset);
                    if (file.fail()) {
                        file.close();
                        json result = {{"filename", part.filename}, {"status", "failed"}, {"message", "Cannot seek to offset"}};
                        results.push_back(result);
                        continue;
                    }

                    // Write the data at the given offset
                    file.write(part.data, part.len);
                    file.close();

                    // Record success result
                    json result = {{"filename", part.filename}, {"status", "success"}, {"size", part.len}, {"offset", offset}, {"storage", effectiveStorage}};
                    results.push_back(result);
                    successCount++;
                } catch (const std::exception& e) {
                    json result = {{"filename", part.filename}, {"status", "failed"}, {"message", "Upload error: " + std::string(e.what())}};
                    results.push_back(result);
                }
            }

            // Prepare response data
            json data       = json::object();
            data["uploads"] = results;
            data["count"]   = successCount;
            data["storage"] = effectiveStorage;

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
            std::string path    = getParam(req, "path");
            std::string storage = getParam(req, "storage");
            path                = decodePath(path);

            // Default to local if storage is empty
            std::string effectiveStorage = storage.empty() ? "local" : storage;

            if (path.empty()) {
                response(res, -1, "Path is empty.");
                return API_STATUS_OK;
            }

            if (!isValidStorage(effectiveStorage)) {
                response(res, -1, "Invalid storage parameter. Use 'local' or 'sd'.");
                return API_STATUS_OK;
            }
            if (effectiveStorage == "sd" && !isSDAvailable()) {
                response(res, -1, "SD card not available.");
                return API_STATUS_OK;
            }

            if (!isValidPath(path)) {
                response(res, -1, "Invalid path.");
                return API_STATUS_OK;
            }

            std::string fullPath = getFullPath(path, effectiveStorage);

            if (!std::filesystem::exists(fullPath)) {
                response(res, -1, "File does not exist.");
                return API_STATUS_OK;
            }

            if (!std::filesystem::is_regular_file(fullPath)) {
                response(res, -1, "Path is not a file.");
                return API_STATUS_OK;
            }

            json data       = json::object();
            data["file"]    = fullPath;
            data["storage"] = effectiveStorage;
            response(res, 0, "File ready.", data);
            return API_STATUS_REPLY_FILE;
        } catch (const std::filesystem::filesystem_error& e) {
            response(res, -1, "Filesystem error: " + std::string(e.what()));
        } catch (std::exception& e) {
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
            std::string storage = getParam(req, "storage");

            // Default to local if storage is empty
            std::string effectiveStorage = storage.empty() ? "local" : storage;

            if (oldPath.empty() || newPath.empty()) {
                response(res, -1, "Paths are empty.");
                return API_STATUS_OK;
            }

            if (!isValidStorage(effectiveStorage)) {
                response(res, -1, "Invalid storage parameter. Use 'local' or 'sd'.");
                return API_STATUS_OK;
            }
            if (effectiveStorage == "sd" && !isSDAvailable()) {
                response(res, -1, "SD card not available.");
                return API_STATUS_OK;
            }

            if (!isValidPath(oldPath) || !isValidPath(newPath)) {
                response(res, -1, "Invalid paths.");
                return API_STATUS_OK;
            }

            std::string fullOldPath = getFullPath(oldPath, effectiveStorage);
            std::string fullNewPath = getFullPath(newPath, effectiveStorage);

            if (!std::filesystem::exists(fullOldPath)) {
                response(res, -1, "Source does not exist.");
                return API_STATUS_OK;
            }

            if (std::filesystem::exists(fullNewPath)) {
                response(res, -1, "Destination already exists.");
                return API_STATUS_OK;
            }

            std::filesystem::rename(fullOldPath, fullNewPath);
            json data        = json::object();
            data["old_path"] = oldPath;
            data["new_path"] = newPath;
            data["storage"]  = effectiveStorage;
            response(res, 0, "Renamed successfully.", data);
        } catch (const std::filesystem::filesystem_error& e) {
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
            std::string path    = getParam(req, "path");
            std::string storage = getParam(req, "storage");
            path                = decodePath(path);

            // Default to local if storage is empty
            std::string effectiveStorage = storage.empty() ? "local" : storage;

            if (path.empty()) {
                response(res, -1, "Path is empty.");
                return API_STATUS_OK;
            }

            if (!isValidStorage(effectiveStorage)) {
                response(res, -1, "Invalid storage parameter. Use 'local' or 'sd'.");
                return API_STATUS_OK;
            }
            if (effectiveStorage == "sd" && !isSDAvailable()) {
                response(res, -1, "SD card not available.");
                return API_STATUS_OK;
            }

            if (!isValidPath(path)) {
                response(res, -1, "Invalid path.");
                return API_STATUS_OK;
            }

            std::string fullPath = getFullPath(path, effectiveStorage);

            if (!std::filesystem::exists(fullPath)) {
                response(res, -1, "Path does not exist.");
                return API_STATUS_OK;
            }

            json data            = json::object();
            data["path"]         = path;
            data["is_directory"] = std::filesystem::is_directory(fullPath);
            data["is_file"]      = std::filesystem::is_regular_file(fullPath);
            data["storage"]      = effectiveStorage;

            if (std::filesystem::is_regular_file(fullPath)) {
                data["size"] = static_cast<uint64_t>(std::filesystem::file_size(fullPath));
            } else {
                data["size"] = 0;
            }

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
            // std::string dir = script(__func__);
            // if (!dir.empty()) {
            //     _local_dir = dir;
            // }

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