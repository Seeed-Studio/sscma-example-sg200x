
#include "server.h"

#include "camera.h"
#include "model.h"
#include "save.h"
#include "stream.h"

namespace ma::node {

static constexpr char TAG[] = "ma::node::server";

void NodeServer::onConnect(struct mosquitto* mosq, int rc) {
    std::string topic = m_topic_in_prefix + "/+";
    mosquitto_subscribe(mosq, NULL, m_topic_in_prefix.c_str(), 0);
    mosquitto_subscribe(mosq, NULL, topic.c_str(), 0);
    m_connected.store(true);
    MA_LOGI(TAG, "node server connected");
    response("", json::object({{"type", MA_MSG_TYPE_RESP}, {"name", "node"}, {"code", MA_OK}, {"data", ""}}));
}

void NodeServer::onDisconnect(struct mosquitto* mosq, int rc) {
    m_connected.store(false);
}

void NodeServer::onMessage(struct mosquitto* mosq, const struct mosquitto_message* msg) {
    std::string topic = msg->topic;
    std::string id    = "";
    Exception e(MA_OK, "");
    json payload;
    MA_TRY {
        id      = topic.substr(m_topic_in_prefix.length() + 1);
        payload = json::parse(static_cast<const char*>(msg->payload), nullptr, false);
        if (payload.is_discarded() || !payload.contains("name") || !payload.contains("data")) {
            e = Exception(MA_EINVAL, "invalid payload");
            MA_THROW(e);
        }
        MA_LOGV(TAG, "request: %s <== %s", id.c_str(), payload.dump().c_str());
        m_executor.submit([this, id = std::move(id), payload = std::move(payload)]() -> bool {
            Exception e(MA_OK, "");
            std::string name = payload["name"].get<std::string>();
            std::transform(name.begin(), name.end(), name.begin(), ::tolower);
            const json& data = payload["data"];
            MA_TRY {
                if (name == "create") {
                    if (!data.contains("type")) {
                        e = Exception(MA_EINVAL, "invalid payload");
                        MA_THROW(e);
                    }
                    std::string type = data["type"].get<std::string>();
                    if (NodeFactory::create(id, type, data, this) == nullptr) {
                        e = Exception(MA_EINVAL, "invalid payload");
                        MA_THROW(Exception(MA_EINVAL, "invalid payload"));
                    }
                } else if (name == "destroy") {
                    NodeFactory::destroy(id);
                    this->response(id, json::object({{"type", MA_MSG_TYPE_RESP}, {"name", name}, {"code", MA_OK}, {"data", ""}}));
                } else if (name == "clear") {
                    MA_LOGD(TAG, "clear all nodes");
                    NodeFactory::clear();
                    this->response(id, json::object({{"type", MA_MSG_TYPE_RESP}, {"name", name}, {"code", MA_OK}, {"data", ""}}));
                } else if (name == "health") {
                    this->response(id, json::object({{"type", MA_MSG_TYPE_RESP}, {"name", name}, {"code", MA_OK}, {"data", ""}}));
                } else {
                    Node* node = NodeFactory::find(id);
                    if (node) {
                        node->onControl(payload["name"].get<std::string>(), data);
                    }
                }
            }
            MA_CATCH(const Exception& e) {
                if (e.err() != MA_AGAIN) {
                    this->response(id, json::object({{"type", MA_MSG_TYPE_RESP}, {"name", name}, {"code", e.err()}, {"data", e.what()}}));
                }
                return e.err() == MA_AGAIN;
            }
            MA_CATCH(const std::exception& e) {
                MA_LOGE(TAG, "onMessage exception: %s", e.what());
                this->response(id, json::object({{"type", MA_MSG_TYPE_RESP}, {"name", name}, {"code", MA_EINVAL}, {"data", e.what()}}));
                return false;
            }
            return false;
        });
    }
    MA_CATCH(const Exception& e) {
        response(id, json::object({{"type", MA_MSG_TYPE_RESP}, {"name", "request"}, {"code", e.err()}, {"data", e.what()}}));
        return;
    }
    MA_CATCH(const std::exception& e) {
        response(id, json::object({{"type", MA_MSG_TYPE_RESP}, {"name", "request"}, {"code", MA_EINVAL}, {"data", e.what()}}));
        return;
    }
}

void NodeServer::onConnectStub(struct mosquitto* mosq, void* obj, int rc) {
    NodeServer* server = static_cast<NodeServer*>(obj);
    if (server) {
        server->onConnect(mosq, rc);
    }
}

void NodeServer::onDisconnectStub(struct mosquitto* mosq, void* obj, int rc) {

    NodeServer* server = static_cast<NodeServer*>(obj);
    if (server) {
        server->onDisconnect(mosq, rc);
    }
}

void NodeServer::onMessageStub(struct mosquitto* mosq, void* obj, const struct mosquitto_message* msg) {

    NodeServer* server = static_cast<NodeServer*>(obj);
    if (server) {
        server->onMessage(mosq, msg);
    }
}

void NodeServer::response(const std::string& id, const json& msg) {

    if (!m_connected) {
        return;
    }
    // Guard guard(m_mutex);
    std::string topic = m_topic_out_prefix + '/' + id;
    MA_LOGV(TAG, "response: %s ==> %s", id.c_str(), msg.dump().c_str());
    int mid = mosquitto_publish(m_client, nullptr, topic.c_str(), msg.dump().size(), msg.dump().data(), 0, false);
    return;
}

void NodeServer::setStorage(StorageFile* storage) {
    m_storage = storage;
}

StorageFile* NodeServer::getStorage() const {
    return m_storage;
}

NodeServer::NodeServer(std::string client_id) : m_client(nullptr), m_connected(false), m_client_id(std::move(client_id)), m_storage(nullptr), m_mutex() {
    mosquitto_lib_init();

    m_client = mosquitto_new(m_client_id.c_str(), true, this);
    MA_ASSERT(m_client);

    mosquitto_connect_callback_set(m_client, onConnectStub);
    mosquitto_disconnect_callback_set(m_client, onDisconnectStub);
    mosquitto_message_callback_set(m_client, onMessageStub);

    m_topic_in_prefix  = std::string("sscma/v0/" + m_client_id + "/node/in");
    m_topic_out_prefix = std::string("sscma/v0/" + m_client_id + "/node/out");

#if MA_USE_NODE_REGISTRAR == 0
    NodeFactory::registerNode("camera", [](const std::string& id) { return new CameraNode(id); });
    NodeFactory::registerNode("model", [](const std::string& id) { return new ModelNode(id); });
    NodeFactory::registerNode("save", [](const std::string& id) { return new SaveNode(id); });
    NodeFactory::registerNode("stream", [](const std::string& id) { return new StreamNode(id); });
#endif
}
NodeServer::~NodeServer() {
    stop();
    if (m_client) {
        mosquitto_destroy(m_client);
    }
    mosquitto_lib_cleanup();
}

ma_err_t NodeServer::start(std::string host, int port, std::string username, std::string password) {

    if (m_connected.load()) {
        return MA_EBUSY;
    }

    mosquitto_loop_start(m_client);

    mosquitto_reconnect_delay_set(m_client, 2, 30, true);
    if (username.length() > 0 && password.length() > 0) {
        mosquitto_username_pw_set(m_client, username.c_str(), password.c_str());
    }
    int rc = mosquitto_connect(m_client, host.c_str(), port, 60);

    MA_LOGI(TAG, "node server started: mqtt://%s:%d", host.c_str(), port);
    MA_LOGI(TAG, "in: %s, out: %s", m_topic_in_prefix.c_str(), m_topic_out_prefix.c_str());


    return m_client && rc == MOSQ_ERR_SUCCESS ? MA_OK : MA_AGAIN;
}

ma_err_t NodeServer::stop() {
    if (m_client && m_connected.load()) {
        mosquitto_disconnect(m_client);
        mosquitto_loop_stop(m_client, true);
    }
    return MA_OK;
}

}  // namespace ma::node