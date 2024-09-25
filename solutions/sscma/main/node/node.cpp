#include "node.h"

namespace ma::node {

static constexpr char TAG[] = "ma::node";

Node::Node(std::string type, std::string id)
    : mutex_(),
      id_(std::move(id)),
      type_(std::move(type)),
      status_(NODE_STATUS_UNKNOWN),
      dependencies_() {
    MA_LOGD(TAG, "create node: %s(%s) %p", type_.c_str(), id_.c_str(), &mutex_);
}

Node::~Node() = default;

Mutex NodeFactory::m_mutex;
std::unordered_map<std::string, Node*> NodeFactory::m_nodes;


Node* NodeFactory::create(const std::string id, const std::string type, const json& data) {

    Guard guard(m_mutex);
    std::string _type = type;
    std::transform(_type.begin(), _type.end(), _type.begin(), ::tolower);

    if (m_nodes.find(id) != m_nodes.end()) {
        return nullptr;
    }

    auto it = registry().find(_type);

    if (it == registry().end()) {
        MA_LOGE(TAG, "unknown node type: %s", _type.c_str());
        return nullptr;
    }

    if (it->second.singleton) {
        for (auto node : m_nodes) {
            if (node.second->type_ == _type) {
                MA_LOGE(TAG, "singleton node already exists: %s", _type.c_str());
                return nullptr;
            }
        }
    }

    Node* n = it->second.create(id);

    if (data.contains("dependencies")) {
        for (auto dep : data["dependencies"].get<std::vector<std::string>>()) {
            n->dependencies_[dep] = find(dep);
        }
    }

    m_nodes[id] = n;

    if (MA_OK != n->onCreate(data.contains("config") ? data["config"] : json::object())) {
        delete n;
        m_nodes.erase(id);
        MA_LOGE(TAG, "create node failed: %s", id.c_str());
        return nullptr;
    }

    return n;
}

void NodeFactory::destroy(const std::string id) {
    Guard guard(m_mutex);

    auto node = m_nodes.find(id);
    if (node == m_nodes.end()) {
        return;
    }

    node->second->onDestroy();

    delete node->second;
    m_nodes.erase(id);

    return;
}

Node* NodeFactory::find(const std::string id) {
    auto node = m_nodes.find(id);
    if (node == m_nodes.end()) {
        return nullptr;
    }
    return node->second;
}

void NodeFactory::clear() {
    Guard guard(m_mutex);
    for (auto node : m_nodes) {
        node.second->onDestroy();
        delete node.second;
    }
    m_nodes.clear();
}

void NodeFactory::registerNode(const std::string type, CreateNode create, bool singleton) {
    MA_LOGI(TAG, "register node: %s %s", type.c_str(), singleton ? "singleton" : "");
    std::string _type = type;
    std::transform(_type.begin(), _type.end(), _type.begin(), ::tolower);
    registry()[_type] = {create, singleton};
}

std::unordered_map<std::string, NodeFactory::NodeCreator>& NodeFactory::registry() {
    static std::unordered_map<std::string, NodeFactory::NodeCreator> _registry;
    return _registry;
}


}  // namespace ma::node
