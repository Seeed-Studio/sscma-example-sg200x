#include "ha_mqtt.h"

#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

#include <unistd.h>

#include <mosquitto.h>

#define TAG "ha_mqtt"

// Standalone logging (component must not depend on sscma-micro headers).
#define MA_LOGI(tag, fmt, ...) fprintf(stderr, "I [%s] " fmt "\n", tag, ##__VA_ARGS__)
#define MA_LOGW(tag, fmt, ...) fprintf(stderr, "W [%s] " fmt "\n", tag, ##__VA_ARGS__)
#define MA_LOGE(tag, fmt, ...) fprintf(stderr, "E [%s] " fmt "\n", tag, ##__VA_ARGS__)

namespace ha_mqtt {

// ---------------------------------------------------------------------------
// ha.conf parsing
// ---------------------------------------------------------------------------

static std::string trim(const std::string& s) {
    size_t b = 0, e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

// Unquote a shell-style value. Handles single-quoted values with the
// standard '\'' escape (close quote, literal ', reopen quote) and plain
// backslash escapes outside quotes. Small state machine; never shells out.
// Returns false on unterminated quote.
static bool unquoteValue(const std::string& raw, std::string& out) {
    out.clear();
    bool in_quote = false;
    for (size_t i = 0; i < raw.size(); ++i) {
        char c = raw[i];
        if (in_quote) {
            if (c == '\'') {
                in_quote = false;
            } else {
                out.push_back(c);
            }
        } else {
            if (c == '\'') {
                in_quote = true;
            } else if (c == '\\' && i + 1 < raw.size()) {
                out.push_back(raw[++i]);
            } else {
                out.push_back(c);
            }
        }
    }
    return !in_quote;
}

bool loadHaConfig(const std::string& path, HaConfig& out, std::string* error) {
    out = HaConfig{};

    std::ifstream f(path);
    if (!f.is_open()) {
        if (error) *error = "config file not found: " + path;
        return false;
    }
    out.present = true;

    bool parse_ok = true;
    std::string parse_err;
    std::string line;
    while (std::getline(f, line)) {
        std::string t = trim(line);
        if (t.empty() || t[0] == '#') continue;

        size_t eq = t.find('=');
        if (eq == std::string::npos) continue;  // not a KEY=VALUE line, skip

        std::string key = trim(t.substr(0, eq));
        std::string raw_value = trim(t.substr(eq + 1));

        // Whitelist of accepted keys; anything else is silently ignored.
        if (key != "HA_ENABLED" && key != "HA_BROKER_HOST" && key != "HA_BROKER_PORT" &&
            key != "HA_USERNAME" && key != "HA_PASSWORD" && key != "HA_DISCOVERY_PREFIX") {
            continue;
        }

        std::string value;
        if (!unquoteValue(raw_value, value)) {
            parse_ok = false;
            parse_err = "unterminated quote for key " + key;
            break;
        }

        if (key == "HA_ENABLED") {
            out.enabled = (value == "1");
        } else if (key == "HA_BROKER_HOST") {
            out.broker_host = value;
        } else if (key == "HA_BROKER_PORT") {
            char* end = nullptr;
            long port = std::strtol(value.c_str(), &end, 10);
            if (end == value.c_str() || *end != '\0' || port < 1 || port > 65535) {
                parse_ok = false;
                parse_err = "invalid HA_BROKER_PORT: " + value;
                break;
            }
            out.broker_port = static_cast<uint16_t>(port);
        } else if (key == "HA_USERNAME") {
            out.username = value;
        } else if (key == "HA_PASSWORD") {
            out.password = value;
        } else if (key == "HA_DISCOVERY_PREFIX") {
            if (!value.empty()) out.discovery_prefix = value;
        }
    }

    if (parse_ok && out.enabled && out.broker_host.empty()) {
        parse_ok = false;
        parse_err = "HA_ENABLED=1 but HA_BROKER_HOST is empty";
    }

    if (!parse_ok) {
        if (error) *error = parse_err;
        out.enabled = false;  // fall back to legacy mode
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Device identifier
// ---------------------------------------------------------------------------

std::string readDeviceIdentifier() {
    // Primary: U-Boot env serial number (mirrors supervisor main.sh _sn()).
    FILE* p = popen("fw_printenv sn 2>/dev/null", "r");
    if (p) {
        char buf[256] = {0};
        std::string output;
        while (fgets(buf, sizeof(buf), p)) output += buf;
        pclose(p);
        // Output form: "sn=XXXX" -> take everything after the last '='.
        size_t eq = output.rfind('=');
        std::string sn = trim(eq == std::string::npos ? output : output.substr(eq + 1));
        if (!sn.empty()) return sn;
    }

    // Fallback: WLAN MAC, colons stripped, lowercased.
    std::ifstream mac_file("/sys/class/net/wlan0/address");
    if (mac_file.is_open()) {
        std::string mac;
        std::getline(mac_file, mac);
        std::string id;
        for (char c : mac) {
            if (c == ':') continue;
            id.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        }
        id = trim(id);
        if (!id.empty()) return id;
    }

    return "unknown";
}

// ---------------------------------------------------------------------------
// JSON helpers
// ---------------------------------------------------------------------------

static std::string jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out.push_back(c);
                }
        }
    }
    return out;
}

// ---------------------------------------------------------------------------
// MqttPublisher
// ---------------------------------------------------------------------------

MqttPublisher::MqttPublisher()
    : client_(nullptr), connected_(false), initialized_(false) {}

MqttPublisher::~MqttPublisher() {
    deinit();
}

bool MqttPublisher::init(const ClientOptions& options) {
    if (initialized_.load()) {
        MA_LOGW(TAG, "Already initialized");
        return true;
    }

    options_ = options;

    std::string err;
    if (!loadHaConfig(options_.config_path, ha_, &err)) {
        MA_LOGI(TAG, "HA config unavailable (%s), using legacy mode", err.c_str());
    }
    if (ha_.present && !ha_.enabled) {
        MA_LOGI(TAG, "HA integration disabled in %s, using legacy mode", options_.config_path.c_str());
    }

    sn_ = options_.device_identifier.empty() ? readDeviceIdentifier() : options_.device_identifier;

    // Resolve a friendly device name for HA discovery + results payload.
    // The compiled-in default is "reCamera"; when left at the default (or
    // empty) fall back to the actual hostname, which supervisor keeps in sync
    // with the user-set device name (updateDeviceName -> hostname + avahi).
    // device.identifiers stays recamera_<sn> (stable unique key); only the
    // human-facing name changes.
    if (options_.device_name.empty() || options_.device_name == "reCamera") {
        char host[256] = {0};
        if (gethostname(host, sizeof(host) - 1) == 0 && host[0] != '\0') {
            options_.device_name = host;
        } else {
            options_.device_name = "reCamera";
        }
    }

    mosquitto_lib_init();

    client_ = mosquitto_new(options_.client_id.c_str(), true, this);
    if (!client_) {
        MA_LOGE(TAG, "Failed to create MQTT client");
        mosquitto_lib_cleanup();
        return false;
    }

    mosquitto_connect_callback_set(client_, onConnectCallback);
    mosquitto_disconnect_callback_set(client_, onDisconnectCallback);
    mosquitto_publish_callback_set(client_, onPublishCallback);
    mosquitto_message_callback_set(client_, onMessageCallback);

    std::string host;
    uint16_t port;
    if (ha_.enabled) {
        host = ha_.broker_host;
        port = ha_.broker_port;

        if (!ha_.username.empty()) {
            mosquitto_username_pw_set(client_, ha_.username.c_str(),
                                      ha_.password.empty() ? nullptr : ha_.password.c_str());
        }

        // Availability LWT: broker marks us offline on unclean disconnect.
        const std::string status_topic = statusTopic();
        static const char kOffline[] = "offline";
        mosquitto_will_set(client_, status_topic.c_str(),
                           static_cast<int>(sizeof(kOffline) - 1), kOffline, 1, true);
    } else {
        host = options_.legacy_host;
        port = options_.legacy_port;
    }

    mosquitto_reconnect_delay_set(client_, 2, 30, true);

    int rc = mosquitto_loop_start(client_);
    if (rc != MOSQ_ERR_SUCCESS) {
        MA_LOGE(TAG, "Failed to start MQTT loop: %d", rc);
        mosquitto_destroy(client_);
        client_ = nullptr;
        mosquitto_lib_cleanup();
        return false;
    }

    rc = mosquitto_connect(client_, host.c_str(), port, 60);
    if (rc != MOSQ_ERR_SUCCESS) {
        MA_LOGW(TAG, "Initial connection failed (will retry): %d", rc);
    }

    initialized_.store(true);
    MA_LOGI(TAG, "MQTT publisher initialized: %s:%d (%s mode, device recamera_%s)",
            host.c_str(), port, ha_.enabled ? "HA" : "legacy", sn_.c_str());
    MA_LOGI(TAG, "Results topic: %s", options_.results_topic.c_str());
    return true;
}

void MqttPublisher::deinit() {
    if (!initialized_.load()) return;

    if (client_) {
        if (ha_.enabled && connected_.load()) {
            // Graceful shutdown: LWT does not fire on clean disconnect,
            // so publish offline explicitly (retained).
            const std::string status_topic = statusTopic();
            mosquitto_publish(client_, nullptr, status_topic.c_str(), 7, "offline", 1, true);
        }
        if (connected_.load()) {
            mosquitto_disconnect(client_);
        }
        mosquitto_loop_stop(client_, true);
        mosquitto_destroy(client_);
        client_ = nullptr;
    }

    mosquitto_lib_cleanup();
    initialized_.store(false);
    connected_.store(false);
    MA_LOGI(TAG, "MQTT publisher deinitialized");
}

// ---- callbacks ----

void MqttPublisher::onConnectCallback(struct mosquitto*, void* obj, int rc) {
    if (auto* self = static_cast<MqttPublisher*>(obj)) self->onConnect(rc);
}

void MqttPublisher::onDisconnectCallback(struct mosquitto*, void* obj, int rc) {
    if (auto* self = static_cast<MqttPublisher*>(obj)) self->onDisconnect(rc);
}

void MqttPublisher::onPublishCallback(struct mosquitto*, void* obj, int mid) {
    if (auto* self = static_cast<MqttPublisher*>(obj)) self->onPublish(mid);
}

void MqttPublisher::onMessageCallback(struct mosquitto*, void* obj,
                                      const struct mosquitto_message* msg) {
    auto* self = static_cast<MqttPublisher*>(obj);
    if (!self || !msg || !msg->topic) return;
    std::string payload;
    if (msg->payload && msg->payloadlen > 0) {
        payload.assign(static_cast<const char*>(msg->payload),
                       static_cast<size_t>(msg->payloadlen));
    }
    self->onMessage(msg->topic, payload);
}

void MqttPublisher::onConnect(int rc) {
    if (rc != 0) {
        MA_LOGE(TAG, "Connection failed with code: %d", rc);
        return;
    }
    connected_.store(true);
    MA_LOGI(TAG, "Connected to MQTT broker");

    if (ha_.enabled) {
        publishAvailability();
        publishDiscoveryConfigs();

        // Re-announce when Home Assistant restarts (its birth message).
        const std::string ha_status = ha_.discovery_prefix + "/status";
        int rc2 = mosquitto_subscribe(client_, nullptr, ha_status.c_str(), 0);
        if (rc2 != MOSQ_ERR_SUCCESS) {
            MA_LOGW(TAG, "Failed to subscribe %s: %d", ha_status.c_str(), rc2);
        }
    }

    // Application command topics. Re-subscribed on every connect because a
    // reconnect starts a fresh session (clean_session is the mosquitto default).
    for (const auto& topic : options_.subscribe_topics) {
        if (topic.empty()) continue;
        int rc3 = mosquitto_subscribe(client_, nullptr, topic.c_str(), 1);
        if (rc3 != MOSQ_ERR_SUCCESS) {
            MA_LOGW(TAG, "Failed to subscribe %s: %d", topic.c_str(), rc3);
        } else {
            MA_LOGI(TAG, "Subscribed: %s", topic.c_str());
        }
    }
}

void MqttPublisher::onDisconnect(int rc) {
    connected_.store(false);
    if (rc != 0) {
        MA_LOGW(TAG, "Unexpected disconnect: %d", rc);
    } else {
        MA_LOGI(TAG, "Disconnected from MQTT broker");
    }
}

void MqttPublisher::onPublish(int mid) {
    {
        std::lock_guard<std::mutex> lk(ack_mutex_);
        acked_mids_.insert(mid);
        if (acked_mids_.size() > 256) acked_mids_.erase(acked_mids_.begin());
    }
    ack_cv_.notify_all();
}

void MqttPublisher::onMessage(const std::string& topic, const std::string& payload) {
    if (ha_.enabled && topic == ha_.discovery_prefix + "/status") {
        if (payload == "online") {
            MA_LOGI(TAG, "Home Assistant is online, re-publishing discovery");
            publishAvailability();
            publishDiscoveryConfigs();
        }
        return;
    }
    if (options_.message_handler) {
        options_.message_handler(topic, payload);
    }
}

// ---- discovery ----

void MqttPublisher::publishAvailability() {
    const std::string topic = statusTopic();
    int rc = mosquitto_publish(client_, nullptr, topic.c_str(), 6, "online", 1, true);
    if (rc != MOSQ_ERR_SUCCESS) {
        MA_LOGW(TAG, "Failed to publish availability: %d", rc);
    }
}

static const char* componentName(EntityType t) {
    switch (t) {
        case EntityType::BinarySensor: return "binary_sensor";
        case EntityType::Sensor:       return "sensor";
        case EntityType::Image:        return "image";
    }
    return "sensor";
}

std::string MqttPublisher::discoveryTopic(const EntityConfig& e) const {
    return ha_.discovery_prefix + "/" + componentName(e.type) + "/recamera_" + sn_ +
           "/" + e.object_id + "/config";
}

std::string MqttPublisher::discoveryPayload(const EntityConfig& e) const {
    std::ostringstream j;
    j << "{";
    j << "\"name\":\"" << jsonEscape(e.name) << "\",";
    j << "\"unique_id\":\"recamera_" << jsonEscape(sn_) << "_" << jsonEscape(e.object_id) << "\",";
    if (e.type == EntityType::Image) {
        j << "\"image_topic\":\"" << jsonEscape(snapshotTopic()) << "\",";
        j << "\"content_type\":\"image/jpeg\",";
    } else {
        j << "\"state_topic\":\"" << jsonEscape(options_.results_topic) << "\",";
        j << "\"value_template\":\"" << jsonEscape(e.value_template) << "\",";
    }
    if (!e.device_class.empty()) {
        j << "\"device_class\":\"" << jsonEscape(e.device_class) << "\",";
    }
    if (!e.unit_of_measurement.empty()) {
        j << "\"unit_of_measurement\":\"" << jsonEscape(e.unit_of_measurement) << "\",";
    }
    if (!e.state_class.empty()) {
        j << "\"state_class\":\"" << jsonEscape(e.state_class) << "\",";
    }
    j << "\"availability_topic\":\"" << jsonEscape(statusTopic()) << "\",";
    j << "\"payload_available\":\"online\",";
    j << "\"payload_not_available\":\"offline\",";
    j << "\"device\":{";
    j << "\"identifiers\":[\"recamera_" << jsonEscape(sn_) << "\"],";
    j << "\"name\":\"" << jsonEscape(options_.device_name) << "\",";
    j << "\"manufacturer\":\"Seeed Studio\",";
    j << "\"model\":\"reCamera\"";
    j << "}";
    j << "}";
    return j.str();
}

void MqttPublisher::publishDiscoveryConfigs() {
    for (const auto& e : options_.entities) {
        const std::string topic = discoveryTopic(e);
        const std::string payload = discoveryPayload(e);
        int rc = mosquitto_publish(client_, nullptr, topic.c_str(),
                                   static_cast<int>(payload.size()), payload.data(), 1, true);
        if (rc != MOSQ_ERR_SUCCESS) {
            MA_LOGW(TAG, "Failed to publish discovery config %s: %d", topic.c_str(), rc);
        } else {
            MA_LOGI(TAG, "Discovery config published: %s", topic.c_str());
        }
    }
}

// ---- publishing ----

bool MqttPublisher::publishResultsJson(const std::string& payload) {
    // Inject a top-level "device" field so bare MQTT consumers can identify the
    // source device. Back-compatible: an extra leading field only; all existing
    // fields are preserved untouched.
    std::string out;
    size_t brace = payload.find('{');
    if (brace == std::string::npos) {
        out = payload;  // not a JSON object; leave as-is
    } else {
        size_t next = payload.find_first_not_of(" \t\r\n", brace + 1);
        bool empty_obj = (next == std::string::npos || payload[next] == '}');
        out.reserve(payload.size() + options_.device_name.size() + 16);
        out.append(payload, 0, brace + 1);
        out += "\"device\":\"";
        out += jsonEscape(options_.device_name);
        out += "\"";
        if (!empty_obj) out += ",";
        out.append(payload, brace + 1, std::string::npos);
    }
    return publishText(options_.results_topic, out,
                       options_.results_qos, options_.results_retain);
}

bool MqttPublisher::publishText(const std::string& topic, const std::string& payload,
                                int qos, bool retain) {
    if (!initialized_.load() || !client_) return false;

    int rc = mosquitto_publish(client_, nullptr, topic.c_str(),
                               static_cast<int>(payload.size()), payload.data(), qos, retain);
    if (rc != MOSQ_ERR_SUCCESS) {
        MA_LOGE(TAG, "Publish failed: rc=%d, connected=%d, topic=%s, size=%d",
                rc, connected_.load() ? 1 : 0, topic.c_str(), (int)payload.size());
        return false;
    }
    return true;
}

bool MqttPublisher::publishBinary(const std::string& topic, const void* data, size_t len,
                                  int qos, bool retain, int ack_timeout_ms) {
    if (!initialized_.load() || !client_) return false;

    int mid = 0;
    int rc = mosquitto_publish(client_, &mid, topic.c_str(),
                               static_cast<int>(len), data, qos, retain);
    if (rc != MOSQ_ERR_SUCCESS) {
        MA_LOGE(TAG, "Binary publish failed: rc=%d, topic=%s, size=%zu", rc, topic.c_str(), len);
        return false;
    }

    if (qos >= 1 && ack_timeout_ms > 0) {
        std::unique_lock<std::mutex> lk(ack_mutex_);
        bool acked = ack_cv_.wait_for(lk, std::chrono::milliseconds(ack_timeout_ms),
                                      [this, mid] { return acked_mids_.count(mid) > 0; });
        if (acked) {
            acked_mids_.erase(mid);
        } else {
            MA_LOGW(TAG, "PUBACK timeout (%dms) for topic %s (mid=%d)",
                    ack_timeout_ms, topic.c_str(), mid);
            return false;
        }
    }
    return true;
}

}  // namespace ha_mqtt
