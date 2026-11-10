#ifndef _HA_MQTT_H_
#define _HA_MQTT_H_

// Home Assistant MQTT Discovery publisher (shared component).
//
// Two operating modes, chosen at init() from /userdata/local/ha.conf:
//   - HA mode     : HA_ENABLED=1 in the config file. Connects to the configured
//                   broker, sets an availability LWT, publishes retained
//                   discovery configs for the declared entities and re-publishes
//                   them whenever Home Assistant announces its birth message on
//                   <discovery_prefix>/status.
//   - legacy mode : config file missing / disabled / unparsable. Behaves like
//                   the historical per-app mqtt_publisher: plain connection to
//                   localhost (or CLI-overridden host) with async reconnect.

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <set>
#include <string>
#include <vector>

struct mosquitto;
struct mosquitto_message;

namespace ha_mqtt {

enum class EntityType { BinarySensor, Sensor, Image };

struct EntityConfig {
    EntityType type;
    std::string object_id;
    std::string name;
    std::string value_template;
    std::string device_class;         // optional
    std::string unit_of_measurement;  // optional
    std::string state_class;          // optional
};

struct HaConfig {
    bool present = false;   // config file exists and was readable
    bool enabled = false;   // HA_ENABLED=1
    std::string broker_host;
    std::string username;
    std::string password;
    uint16_t broker_port = 1883;
    std::string discovery_prefix = "homeassistant";
};

struct ClientOptions {
    std::string app_id;         // e.g. "facemesh-reader" (topic namespace)
    std::string client_id;      // MQTT client id
    std::string results_topic;  // state_topic for discovery entities
    std::string legacy_host = "localhost";
    uint16_t legacy_port = 1883;
    int results_qos = 0;
    bool results_retain = false;
    std::string config_path = "/userdata/local/ha.conf";
    std::string device_identifier;        // empty -> readDeviceIdentifier()
    std::string device_name = "reCamera";
    std::vector<EntityConfig> entities;

    // Inbound command channel (optional; default empty = publish-only, which is
    // how every existing caller behaves). Every topic listed here is subscribed
    // on each successful connect, and `message_handler` is invoked for every
    // message that is not the Home Assistant birth announcement.
    //
    // The handler runs on the Mosquitto network thread: parse and enqueue only.
    std::vector<std::string> subscribe_topics;
    std::function<void(const std::string& topic, const std::string& payload)> message_handler;
};

// Parse /userdata/local/ha.conf (KEY=VALUE lines, shell-style single-quoted
// values supported). Returns false on missing file or parse error; `out` is
// always left in a consistent state (enabled=false on failure).
bool loadHaConfig(const std::string& path, HaConfig& out, std::string* error = nullptr);

// Device serial number: `fw_printenv sn`, falling back to the wlan0 MAC
// address with colons stripped and lowercased. Never empty ("unknown" fallback).
std::string readDeviceIdentifier();

class MqttPublisher final {
public:
    MqttPublisher();
    ~MqttPublisher();

    MqttPublisher(const MqttPublisher&) = delete;
    MqttPublisher& operator=(const MqttPublisher&) = delete;

    bool init(const ClientOptions& options);
    void deinit();

    bool isConnected() const { return connected_.load(); }
    bool haEnabled() const { return ha_.enabled; }

    // Friendly device name resolved at init() (hostname, or "reCamera"
    // fallback). Used for HA discovery device.name and the results "device"
    // field.
    std::string deviceName() const { return options_.device_name; }

    // Publish a results JSON payload to options.results_topic.
    bool publishResultsJson(const std::string& payload);

    // Publish arbitrary text to a topic.
    bool publishText(const std::string& topic, const std::string& payload,
                     int qos = 0, bool retain = false);

    // Publish binary data (e.g. JPEG). When qos >= 1 and ack_timeout_ms > 0,
    // blocks until the broker PUBACK arrives or the timeout expires; returns
    // false on timeout / publish failure.
    bool publishBinary(const std::string& topic, const void* data, size_t len,
                       int qos = 0, bool retain = false, int ack_timeout_ms = 0);

    std::string snapshotTopic() const { return "recamera/" + options_.app_id + "/snapshot"; }
    std::string statusTopic() const { return "recamera/" + options_.app_id + "/status"; }

private:
    static void onConnectCallback(struct mosquitto* mosq, void* obj, int rc);
    static void onDisconnectCallback(struct mosquitto* mosq, void* obj, int rc);
    static void onPublishCallback(struct mosquitto* mosq, void* obj, int mid);
    static void onMessageCallback(struct mosquitto* mosq, void* obj,
                                  const struct mosquitto_message* msg);

    void onConnect(int rc);
    void onDisconnect(int rc);
    void onPublish(int mid);
    void onMessage(const std::string& topic, const std::string& payload);

    void publishAvailability();
    void publishDiscoveryConfigs();
    std::string discoveryTopic(const EntityConfig& e) const;
    std::string discoveryPayload(const EntityConfig& e) const;

    struct mosquitto* client_;
    ClientOptions options_;
    HaConfig ha_;
    std::string sn_;
    std::atomic<bool> connected_;
    std::atomic<bool> initialized_;

    // PUBACK tracking for publishBinary(ack_timeout_ms > 0)
    std::mutex ack_mutex_;
    std::condition_variable ack_cv_;
    std::set<int> acked_mids_;
};

}  // namespace ha_mqtt

#endif  // _HA_MQTT_H_
