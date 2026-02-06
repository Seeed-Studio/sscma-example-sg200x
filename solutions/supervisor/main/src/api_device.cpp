#include "api_device.h"
#include "api_file.h"
#include <iterator>

api_status_t api_device::getCameraWebsocketUrl(request_t req, response_t res)
{
    std::stringstream ss;
    ss << "ws://" << get_host(req) << ":" << _dev_info.value("ws", "");
    response(res, 0, STR_OK, { { "websocketUrl", ss.str() } });
    return API_STATUS_OK;
}

api_status_t api_device::getDeviceInfo(request_t req, response_t res)
{
    json data = json::object();
    data["ip"] = get_host(req);
    data["port"] = get_port(req);
    data["sn"] = _dev_info.value("sn", "");
    data["deviceName"] = _dev_info.value("deviceName", "");
    data["status"] = _dev_info.value("status", 1);
    data["osName"] = _dev_info.value("osName", "");
    data["osVersion"] = _dev_info.value("osVersion", "");
    response(res, 0, STR_OK, data);
    return API_STATUS_OK;
}

api_status_t api_device::getDeviceList(request_t req, response_t res)
{
    using namespace std;
    map<pair<string, string>, map<string, string>> devs_map;
    auto&& dev_list = json::array();
    auto&& list = parse_result(script(__func__), ';');
    for (auto& l : list) {
        if (l.size() < 10 || (l[0][0] != '='))
            continue;
        std::string type = l[1];
        std::string name = l[3];
        std::string service = l[4];
        std::string ip = l[7];
        std::string port = l[8];
        std::string sn = l[9].find("sn=") != std::string::npos ? l[9] : "";
        if (service == "_sscma._tcp" && !sn.empty()) {
            json dev;
            dev["device"] = name;
            dev["type"] = type;
            dev["ip"] = ip;
            dev["port"] = port;
            dev["info"] = sn;
            dev["services"][service] = port;
            dev_list.push_back(dev);
        }
        devs_map[std::make_pair(type, ip)][service] = port;
    }

    for (auto& dev : dev_list) {
        auto it = devs_map.find(std::make_pair(dev["type"], dev["ip"]));
        if (it != devs_map.end()) {
            for (auto& [service, port] : it->second) {
                dev["services"][service] = port;
            }
        }
    }

    response(res, 0, STR_OK, { { "deviceList", dev_list } });
    return API_STATUS_OK;
}

api_status_t api_device::updateDeviceName(request_t req, response_t res)
{
    std::string result = script(__func__, parse_body(req).value("deviceName", ""));
    response(res, (result == STR_OK) ? 0 : -1, result);
    return API_STATUS_OK;
}

api_status_t api_device::queryDeviceInfo(request_t req, response_t res)
{
    auto&& data = parse_result(script(__func__));
    LOGV("queryDeviceInfo: %s", data.dump().c_str());
    data["appName"] = PROJECT_NAME;
    data["ip"] = get_host(req);
    data["port"] = get_port(req);
    data["officialUrl"] = _dev_info.value("url", "");
    data["sn"] = _dev_info.value("sn", "");
    data["osName"] = _dev_info.value("os", "");
    data["osVersion"] = _dev_info.value("ver", "");
    data["cpu"] = _dev_info.value("cpu", "");
    data["ram"] = _dev_info.value("ram", "");
    data["npu"] = _dev_info.value("npu", "");
    data["terminalPort"] = _dev_info.value("ttyd", "");

    {
        std::string model_type = "Basic";
        if (_dev_info.value("canbus", 0)) {
            model_type = "Gimbal";
        }
        if (_dev_info.value("wifi", 0)) {
            model_type += " WiFi";
        }

        // emmc
        uint64_t emmc = 0;
        if (_dev_info.contains("emmc")) {
            if (_dev_info["emmc"].is_number_integer()) {
                emmc = _dev_info["emmc"].get<uint64_t>();
            } else if (_dev_info["emmc"].is_string()) {
                try {
                    emmc = std::stoull(_dev_info["emmc"].get<std::string>());
                } catch (const std::exception& e) {
                    LOGW("Failed to parse Emmc value: %s", e.what());
                }
            }
        }
        emmc = emmc >> 30; // GB
        if (emmc == 0) {
            model_type += " No EMMC";
        } else if (emmc < 8) {
            model_type += " 8G";
        } else if (emmc < 16) {
            model_type += " 16G";
        } else if (emmc < 32) {
            model_type += " 32G";
        } else if (emmc < 64) {
            model_type += " 64G";
        }

        // sensor
        int sensor = _dev_info.value("sensor", 0);
        if (sensor == 0) {
            model_type += " (No Sensor)";
        } else if (sensor == 1) {
            model_type += " (OV5647)";
        } else if (sensor == 2) {
            model_type += " (GC2053)";
        }

        data["type"] = model_type;
    }
    response(res, 0, STR_OK, data);
    return API_STATUS_OK;
}

api_status_t api_device::getSystemStatus(request_t req, response_t res)
{
    int rollback = _dev_info.value("rollback", 0);
    if (rollback)
        response(res, 0, STR_OK, "System has been recovered from damage.");
    else
        response(res, 0, STR_OK, "System is running normally.");
    return API_STATUS_OK;
}

api_status_t api_device::queryServiceStatus(request_t req, response_t res)
{
    json data = json::object();
    data["sscmaNode"] = _serviced->get_sscma_status();
    data["nodeRed"] = _serviced->get_nodered_status();
    data["system"] = 0;
    data["uptime"] = uptime();
    data["timestamp"] = timestamp();
    response(res, 0, STR_OK, data);
    return API_STATUS_OK;
}

api_status_t api_device::setPower(request_t req, response_t res)
{
    auto&& body = parse_body(req);
    int mode = body.value("mode", -1);
    int ret = -1;
    std::string status = "Invalid mode";
    if (mode == 0) {
        status = "poweroff";
        ret = 0;
    } else if (mode == 1) {
        status = "reboot";
        ret = 0;
    }
    response(res, ret, status);
    if (ret == 0)
        system(status.c_str());
    return API_STATUS_OK;
}

// APP
api_status_t api_device::getAppInfo(request_t req, response_t res)
{
    // Todo:
    response(res);
    return API_STATUS_OK;
}

api_status_t api_device::uploadApp(request_t req, response_t res)
{
    // Todo:
    response(res);
    return API_STATUS_OK;
}

// Model
api_status_t api_device::getModelFile(request_t req, response_t res)
{
    std::string file;
    try {
        file = _dev_info["model"]["file"];
        LOGV("getModelFile: %s", file.c_str());
    } catch (const json::exception& e) {
        response(res, -1, "Invalid model file.");
        return API_STATUS_OK;
    }
    response(res, 0, STR_OK, { { "file", file } });
    return API_STATUS_REPLY_FILE;
}

api_status_t api_device::getModelInfo(request_t req, response_t res)
{
    std::string info;
    try {
        info = _dev_info["model"]["info"];
    } catch (const json::exception& e) {
        response(res, -1, "Invalid model info.");
        return API_STATUS_OK;
    }

    std::ifstream file(info);
    if (!file.is_open()) {
        response(res, 0, STR_OK, { { "model_info", "" } });
        return API_STATUS_OK;
    }
    try {
        json data = json::object();
        std::stringstream ss;
        ss << file.rdbuf();
        data["model_info"] = ss.str();
        response(res, 0, STR_OK, data);
    } catch (const json::exception& e) {
        response(res, -1, "Invalid model info.");
    }
    return API_STATUS_OK;
}

api_status_t api_device::getModelList(request_t req, response_t res)
{
    json data = json::object();
    for (auto& entry : std::filesystem::directory_iterator(_model_dir)) {
        if (entry.path().extension() == ".json") {
            auto info = entry.path().string();
            size_t pos = info.find(".json");
            std::string model = info.substr(0, pos) + _model_suffix;
            LOGD("%s", info.c_str());
            if (!std::filesystem::exists(model))
                continue;

            json js = parse_result(std::ifstream(info));
            js["file"] = model;
            js["id"] = js.value("model_id", "");
            js["name"] = js.value("model_name", "");
            js["md5"] = js.value("checksum", "");
            data["list"].push_back(js);
        }
    }
    response(res, 0, STR_OK, data);
    return API_STATUS_OK;
}

api_status_t api_device::uploadModel(request_t req, response_t res)
{
    std::string model, info;
    try {
        model = _dev_info["model"]["file"];
        info = _dev_info["model"]["info"];
    } catch (const json::exception& e) {
        response(res, -1, "Invalid model dir.");
        return API_STATUS_OK;
    }

    // First pass: extract offset and size from multipart data
    long long offset = -1;
    long long total_size = -1;
    auto&& parts = get_multiparts(req);
    
    for (auto& part : parts) {
        if (part.name == "offset" && part.len > 0) {
            try {
                offset = std::stoll(std::string(part.data, part.len));
            } catch (const std::exception& e) {
                LOGW("Failed to parse offset: %s", e.what());
            }
        } else if (part.name == "size" && part.len > 0) {
            try {
                total_size = std::stoll(std::string(part.data, part.len));
            } catch (const std::exception& e) {
                LOGW("Failed to parse size: %s", e.what());
            }
        }
    }
    
    bool is_chunked = (offset >= 0 && total_size > 0);
    LOGD("Upload mode: %s, offset: %lld, size: %lld", is_chunked ? "chunked" : "legacy", offset, total_size);

    bool has_model = false;
    bool has_info = false;
    json data = json::object();
    
    for (auto& part : parts) {
        LOGD("name: %s, filename: %s, len: %d", part.name.c_str(), part.filename.c_str(), part.len);
        if (part.len == 0)
            continue;

        // Skip offset/size parameters
        if (part.name == "offset" || part.name == "size")
            continue;

        std::string path;
        if (part.name == "model_file") {
            path = model;
            has_model = true;
            
            if (is_chunked) {
                // Chunked upload mode
                FILE* file = nullptr;
                if (offset == 0) {
                    // First chunk: create/truncate file
                    file = fopen((path + ".tmp").c_str(), "wb");
                } else {
                    // Subsequent chunks: append at offset
                    file = fopen((path + ".tmp").c_str(), "r+b");
                }
                
                if (!file) {
                    response(res, -1, "Failed to open model file for writing.");
                    return API_STATUS_OK;
                }
                
                if (fseek(file, offset, SEEK_SET) != 0) {
                    fclose(file);
                    response(res, -1, "Failed to seek to offset.");
                    return API_STATUS_OK;
                }
                
                size_t written = fwrite(part.data, 1, part.len, file);
                fclose(file);
                
                if (written != part.len) {
                    response(res, -1, "Failed to write chunk data.");
                    return API_STATUS_OK;
                }
                
                data["offset"] = offset + part.len;
                data["received"] = offset + part.len;
                
                // Check if this is the final chunk
                bool is_final_chunk = (offset + part.len >= total_size);
                if (!is_final_chunk) {
                    // Not final chunk, return success without processing model_info
                    response(res, 0, STR_OK, data);
                    return API_STATUS_OK;
                }
                // If final chunk, continue to process model_info below
            } else {
                // Legacy mode: single upload
                std::ofstream file(path + ".tmp", std::ios::binary | std::ios::trunc);
                if (!file.is_open())
                    continue;
                file.write(part.data, part.len);
                file.close();
            }
            
            data["list"].push_back({ { part.name, path } });
        } else if (part.name == "model_info") {
            path = info;
            has_info = true;
            
            std::ofstream file(path + ".tmp", std::ios::binary | std::ios::trunc);
            if (!file.is_open())
                continue;
            file.write(part.data, part.len);
            file.close();
            data["list"].push_back({ { part.name, path } });
        } else {
            continue;
        }
    }

    // Support flexible upload: model only, info only, or both
    if (!has_model && !has_info) {
        response(res, -1, "No model file or model info provided.");
        return API_STATUS_OK;
    }

    try {
        if (has_model && has_info) {
            // Both model and info uploaded
            std::filesystem::rename(model + ".tmp", model);
            std::filesystem::rename(info + ".tmp", info);
        } else if (has_model && !has_info) {
            // Only model uploaded, keep existing info if present
            std::filesystem::rename(model + ".tmp", model);
        } else if (!has_model && has_info) {
            // Only info uploaded, try to find matching model in preset directory
            auto&& js = parse_result(std::ifstream(info + ".tmp"));
            std::string file = js.value("file", "");
            
            if (file.empty()) {
                // Search for matching model by model_id
                for (auto& entry : std::filesystem::directory_iterator(_model_dir)) {
                    if (entry.path().extension() == ".json") {
                        auto _path = entry.path().string();
                        std::string _model_file = _path.substr(0, _path.find(".json")) + _model_suffix;
                        if (!std::filesystem::exists(_model_file))
                            continue;

                        json _js = parse_result(std::ifstream(_path));
                        if (!_js.value("model_id", "").empty()
                            && _js.value("model_id", "") == js.value("model_id", "")) {
                            file = _model_file;
                            break;
                        }
                    }
                }

                if (file.empty() || !std::filesystem::exists(file)) {
                    std::filesystem::remove(info + ".tmp");
                    response(res, -1, "Model file is missing in model info.");
                    return API_STATUS_OK;
                }
                // Copy preset model to active model path
                std::filesystem::copy_file(file, model, std::filesystem::copy_options::overwrite_existing);
            }
            // Rename info file
            std::filesystem::rename(info + ".tmp", info);
        }
    } catch (const std::exception& e) {
        response(res, -1, "Upload model failed. " + std::string(e.what()));
        return API_STATUS_OK;
    }

    response(res, 0, STR_OK, data);
    return API_STATUS_OK;
}

// Platform
api_status_t api_device::getPlatformInfo(request_t req, response_t res)
{
    auto&& data = parse_result(std::ifstream(_dev_info.value("platform_info", "")));
    if (data.empty() || data.value("platform_info", "").empty()) {
        response(res, -1, "Get platform info failed.");
        return API_STATUS_OK;
    }
    // LOGV("Platform info: %s", data.dump().c_str());
    response(res, 0, STR_OK, data);
    return API_STATUS_OK;
}

api_status_t api_device::savePlatformInfo(request_t req, response_t res)
{
    auto&& body = parse_body(req);
    if (body.empty() || body.value("platform_info", "").empty()) {
        response(res, -1, "Platform info is empty.");
        return API_STATUS_OK;
    }
    // LOGV("Save platform info: %s", body.dump().c_str());
    std::ofstream file(_dev_info.value("platform_info", ""), std::ios::out | std::ios::trunc);
    if (file.is_open() && file << body.dump()) {
        response(res);
    } else {
        response(res, -1, "Save platform info failed.");
    }
    return API_STATUS_OK;
}

// Upgrade
api_status_t api_device::updateChannel(request_t req, response_t res)
{
    auto&& body = parse_body(req);
    auto ch = 0;
    if (body.contains("channel")) {
        if (body["channel"].is_number_integer()) {
            ch = body["channel"].get<int>();
        } else if (body["channel"].is_string()) {
            try {
                ch = std::stoi(body["channel"].get<std::string>());
            } catch (...) {
                ch = 0;
            }
        }
    }
    auto url = body.value("serverUrl", "");
    if (ch != 0) {
        if (!url.empty() && url.compare(0, 7, "http://") != 0 && url.compare(0, 8, "https://") != 0) {
            response(res, -1, "Server url is invalid.");
            return API_STATUS_OK;
        }
    }
    std::string result = script(__func__, ch, url);
    response(res, (result == STR_OK) ? 0 : -1, result);
    return API_STATUS_OK;
}

api_status_t api_device::cancelUpdate(request_t req, response_t res)
{
    script(__func__);
    response(res);
    return API_STATUS_OK;
}

api_status_t api_device::getSystemUpdateVersion(request_t req, response_t res)
{
    auto&& result = script(__func__);
    auto&& data = parse_result(result);
    if (data.empty()) {
        response(res, -1, result);
        return API_STATUS_OK;
    }
    response(res, 0, STR_OK, data);
    return API_STATUS_OK;
}

api_status_t api_device::getUpdateProgress(request_t req, response_t res)
{
    auto&& result = script(__func__);
    auto&& data = parse_result(result);
    if (data.empty()) {
        response(res, -1, result);
        return API_STATUS_OK;
    }
    response(res, 0, STR_OK, data);
    return API_STATUS_OK;
}

api_status_t api_device::updateSystem(request_t req, response_t res)
{
    script(__func__);
    response(res);
    return API_STATUS_OK;
}

// Helper: Check if string starts with prefix (C++17 compatible)
static inline bool starts_with(const std::string& str, const std::string& prefix)
{
    return str.rfind(prefix, 0) == 0;
}

api_status_t api_device::setTimestamp(request_t req, response_t res)
{
    std::string timestamp_val = get_param(req, "timestamp");

    // Try query parameter first
    if (timestamp_val.empty()) {
        auto&& body = parse_body(req);
        if (body.empty()) {
            response(res, -1, "Request body is empty.");
            return API_STATUS_OK;
        }

        // Extract timestamp from JSON body
        timestamp_val = body.value("timestamp", "");
        if (timestamp_val.empty()) {
            response(res, -1, "Timestamp parameter is missing.");
            return API_STATUS_OK;
        }
    }

    // Convert string to long
    long timestamp = std::atol(timestamp_val.c_str());

    // Set system time
    struct timeval tv;
    tv.tv_sec = timestamp;
    tv.tv_usec = 0;

    if (settimeofday(&tv, nullptr) == 0) {
        response(res, 0, STR_OK, { { "timestamp", timestamp } });
    } else {
        response(res, -1, "Failed to set timestamp.");
    }

    return API_STATUS_OK;
}

api_status_t api_device::getTimestamp(request_t req, response_t res)
{
    std::time_t current_time = std::time(nullptr);
    response(res, 0, STR_OK, { { "timestamp", static_cast<long>(current_time) } });
    return API_STATUS_OK;
}

api_status_t api_device::setTimezone(request_t req, response_t res)
{
    auto&& body = parse_body(req);
    std::string timezone = body.value("timezone", "");

    // Validate input
    if (timezone.empty()) {
        response(res, -1, "Timezone parameter is missing.");
        return API_STATUS_OK;
    }

    // Prevent path traversal or invalid names
    if (timezone.find("..") != std::string::npos || starts_with(timezone, "/") || timezone == "localtime" || timezone == "posixrules") {
        response(res, -1, "Invalid timezone name.");
        return API_STATUS_OK;
    }

    // Full path to the timezone file
    std::string tz_path = "/usr/share/zoneinfo/" + timezone;

    // Check if the target file exists and is a regular file
    if (!std::filesystem::exists(tz_path)) {
        response(res, -1, "Invalid timezone: file does not exist.");
        return API_STATUS_OK;
    }
    if (!std::filesystem::is_regular_file(tz_path)) {
        response(res, -1, "Invalid timezone: not a regular file.");
        return API_STATUS_OK;
    }

    try {
        // Remove existing symlink if present
        if (std::filesystem::exists("/etc/localtime")) {
            std::filesystem::remove("/etc/localtime");
        }

        // Create new symlink to the selected timezone
        std::filesystem::create_symlink(tz_path, "/etc/localtime");

        // Optional: persist timezone in /etc/timezone for system consistency
        std::ofstream tzfile("/etc/timezone");
        if (tzfile) {
            tzfile << timezone << std::endl;
            tzfile.close();
        }

        response(res, 0, STR_OK, { { "timezone", timezone } });
    } catch (const std::filesystem::filesystem_error& e) {
        response(res, -1, "Failed to set timezone: " + std::string(e.what()));
    }

    return API_STATUS_OK;
}

api_status_t api_device::getTimezone(request_t req, response_t res)
{
    std::string timezone;

    try {
        auto target = std::filesystem::read_symlink("/etc/localtime");
        std::string target_path = target.string();

        const std::string prefix = "/usr/share/zoneinfo/";

        // If it's an absolute path pointing to zoneinfo
        if (starts_with(target_path, prefix)) {
            timezone = target_path.substr(prefix.length());
        } else {
            // Try to resolve as relative path
            std::filesystem::path abs_target = std::filesystem::absolute(target);
            std::string abs_str = abs_target.string();
            if (starts_with(abs_str, prefix)) {
                timezone = abs_str.substr(prefix.length());
            }
        }

        // Fallback: check TZ environment variable
        if (timezone.empty()) {
            const char* tz_env = std::getenv("TZ");
            if (tz_env && tz_env[0] != '\0') {
                timezone = tz_env;
            }
        }

    } catch (const std::filesystem::filesystem_error& e) {
        response(res, -1, "Failed to read timezone: " + std::string(e.what()));
        return API_STATUS_OK;
    }

    if (timezone.empty()) {
        response(res, -1, "Unable to determine timezone.");
    } else {
        response(res, 0, STR_OK, { { "timezone", timezone } });
    }

    return API_STATUS_OK;
}

api_status_t api_device::getTimezoneList(request_t req, response_t res)
{
    const std::string zoneinfo_dir = "/usr/share/zoneinfo";
    std::vector<std::string> timezones;

    // Lambda to filter out unwanted directories/files
    auto should_exclude = [](const std::string& path) {
        return starts_with(path, "posix/") || // legacy POSIX rules
            starts_with(path, "right/") || // leap second aware zones
            starts_with(path, "SystemV/") || // old US zone names
            starts_with(path, "Etc/") || // often confusing (e.g., GMT+X means UTC-X)
            path == "posixrules" || path == "localtime" || path == "UTC" || path.find(".tab") != std::string::npos;
    };

    try {
        // Recursively iterate over all files under zoneinfo directory
        for (const auto& entry : std::filesystem::recursive_directory_iterator(zoneinfo_dir)) {
            if (entry.is_regular_file()) {
                std::string full_path = entry.path().string();

                // Extract relative timezone name
                size_t pos = full_path.find(zoneinfo_dir);
                if (pos == std::string::npos)
                    continue;

                std::string tz_name = full_path.substr(pos + zoneinfo_dir.length() + 1);

                // Skip filtered paths
                if (should_exclude(tz_name)) {
                    continue;
                }

                timezones.push_back(tz_name);
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        response(res, -1, "Failed to read timezone directory: " + std::string(e.what()));
        return API_STATUS_OK;
    }

    // Return error if no timezones found
    if (timezones.empty()) {
        response(res, -1, "No timezones found.");
        return API_STATUS_OK;
    }

    // Sort for consistent output
    std::sort(timezones.begin(), timezones.end());

    // Build JSON response
    json data = json::object();
    data["timezones"] = timezones;
    response(res, 0, STR_OK, data);

    return API_STATUS_OK;
}

api_status_t api_device::queryBatteryInfo(request_t req, response_t res)
{
    // Lock mutex to prevent concurrent access to GPIO430
    // The lock is held for the entire operation: set HIGH -> read -> set LOW
    std::lock_guard<std::mutex> lock(_battery_mutex);

    json data = json::object();

    // ADC enable GPIO (GPIO430)
    const char* adc_enable_gpio = "/sys/class/gpio/gpio430/value";
    const char* adc_export = "/sys/class/gpio/export";
    const char* adc_direction = "/sys/class/gpio/gpio430/direction";

    // Check if GPIO is already exported
    std::ifstream gpio_check(adc_enable_gpio);
    bool gpio_exported = gpio_check.is_open();
    gpio_check.close();

    if (!gpio_exported) {
        // Export GPIO430
        std::ofstream export_file(adc_export);
        if (export_file.is_open()) {
            export_file << "430";
            export_file.flush();
            export_file.close();
        }

        // Set direction to out
        std::ofstream direction_file(adc_direction);
        if (direction_file.is_open()) {
            direction_file << "out";
            direction_file.flush();
            direction_file.close();
        }
    } else {
        // Ensure direction is out even if already exported (no delay needed)
        std::ofstream direction_file(adc_direction);
        if (direction_file.is_open()) {
            direction_file << "out";
            direction_file.flush();
            direction_file.close();
        }
    }

    // Step 1: Set GPIO430 HIGH to enable ADC
    std::ofstream gpio_value(adc_enable_gpio);
    if (gpio_value.is_open()) {
        gpio_value << "1";
        gpio_value.flush();
        gpio_value.close();
    }

    // Wait for ADC to stabilize
    usleep(15000); // 30ms

    // Step 2: Read raw ADC value multiple times and average
    long raw_sum = 0;
    int samples = 5;
    double scale = 1.0;

    // Read ADC scale factor once
    std::ifstream scale_file("/sys/bus/iio/devices/iio:device0/in_voltage0_scale");
    if (scale_file.is_open()) {
        scale_file >> scale;
        scale_file.close();
    }

    for (int i = 0; i < samples; i++) {
        std::ifstream raw_file("/sys/bus/iio/devices/iio:device0/in_voltage0_raw");
        long raw_value = 0;
        if (raw_file.is_open()) {
            raw_file >> raw_value;
            raw_file.close();
            raw_sum += raw_value;
        }
        if (i < samples - 1) usleep(5000); // 5ms interval between samples
    }

    long raw_avg = raw_sum / samples;

    // Calculate actual voltage: voltage = raw * scale
    // Hardware voltage divider ratio is 1/2, so multiply by 2
    double voltage_mv = raw_avg * scale * 2.0;

    // Step 3: Set GPIO430 LOW to disable ADC
    std::ofstream gpio_value_off(adc_enable_gpio);
    if (gpio_value_off.is_open()) {
        gpio_value_off << "0";
        gpio_value_off.flush();
        gpio_value_off.close();
    }

    // Return voltage in millivolts
    data["voltage"] = (int)voltage_mv;
    data["raw"] = raw_avg;
    data["scale"] = scale;

    LOGD("Battery voltage: %.2f mV (raw_avg=%ld, scale=%.6f)", voltage_mv, raw_avg, scale);

    response(res, 0, STR_OK, data);
    return API_STATUS_OK;
}
