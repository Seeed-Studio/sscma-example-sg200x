#include "node.h"

namespace ma::node {

static constexpr char TAG[] = "ma::node";

Node::Node(std::string type, std::string id) : mutex_(), id_(std::move(id)), type_(std::move(type)), started_(false), dependencies_(), dependents_(), server_(nullptr) {}

Node::~Node() = default;

const std::string& Node::id() const {
    return id_;
}

const std::string& Node::type() const {
    return type_;
}

const std::string Node::dump() const {
    return json({{"id", id_}, {"type", type_}}).dump();
}

Mutex NodeFactory::m_mutex;
std::unordered_map<std::string, Node*> NodeFactory::m_nodes;


Node* NodeFactory::create(const std::string id, const std::string type, const json& data, NodeServer* server) {

    Guard guard(m_mutex);
    std::string _type = type;
    std::transform(_type.begin(), _type.end(), _type.begin(), ::tolower);

    if (m_nodes.find(id) != m_nodes.end()) {
        MA_THROW(Exception(MA_EEXIST, "node already exists: " + id));
        return nullptr;
    }

    // find node type
    auto it = registry().find(_type);
    if (it == registry().end()) {
        MA_THROW(Exception(MA_EINVAL, "unknown node type: " + _type));
        return nullptr;
    }

    // check singleton
    if (it->second.singleton) {
        for (auto node : m_nodes) {
            if (node.second->type_ == _type) {
                MA_THROW(Exception(MA_EEXIST, "singleton node already exists: " + _type));
                return nullptr;
            }
        }
    }

    // create node
    MA_LOGI(TAG, "create node: %s(%s) %s", _type.c_str(), id.c_str(), data.dump().c_str());
    Node* n = it->second.create(id);
    if (!n) {
        MA_THROW(Exception(MA_ENOMEM, "failed to create node: " + _type));
        return nullptr;
    }
    n->server_ = server;
    if (MA_OK != n->onCreate(data["config"])) {
        return nullptr;
    }
    m_nodes[id] = n;

    // set dependencies
    bool ready = true;
    if (data.contains("dependencies")) {
        for (auto dep : data["dependencies"].get<std::vector<std::string>>()) {
            n->dependencies_[dep] = find(dep);
            if (n->dependencies_[dep] == nullptr) {
                ready = false;
            }
        }
    }

    // set dependents
    if (data.contains("dependents")) {
        for (auto dep : data["dependents"].get<std::vector<std::string>>()) {
            n->dependents_[dep] = find(dep);
            if (n->dependents_[dep] == nullptr) {
                ready = false;
            }
        }
    }

    if (ready) {  // all dependencies are ready
        MA_LOGI(TAG, "start node: %s(%s)", n->type_.c_str(), n->id_.c_str());
        n->onStart();
    }

    // set dependencies for other nodes
    for (auto node : m_nodes) {
        if (!node.second->started_) {  // not started yet
            ready = true;
            MA_LOGD(TAG, "check node: %s(%s) %s", node.second->type_.c_str(), node.second->id_.c_str(), id.c_str());
            for (auto& dep : node.second->dependencies_) {
                MA_LOGD(TAG, "dependencies: %s %p", dep.first.c_str(), dep.second);
                if (dep.second == nullptr && dep.first == id) {
                    MA_LOGD(TAG, "dependencies %s ready: %s(%s)", dep.first.c_str(), node.second->type_.c_str(), node.second->id_.c_str());
                    node.second = n;
                }
                if (dep.second == nullptr) {
                    MA_LOGW(TAG, "dependencies %s not ready: %s(%s)", dep.first.c_str(), node.second->type_.c_str(), node.second->id_.c_str());
                    ready = false;
                }
            }
            for (auto& dep : node.second->dependents_) {
                MA_LOGD(TAG, "dependents: %s %p", dep.first.c_str(), dep.second);
                if (dep.second == nullptr && dep.first == id) {
                    MA_LOGD(TAG, "dependents %s ready: %s(%s)", dep.first.c_str(), node.second->type_.c_str(), node.second->id_.c_str());
                    node.second->dependents_[dep.first] = n;
                }
                if (dep.second == nullptr) {
                    MA_LOGW(TAG, "dependents %s not ready: %s(%s) ", dep.first.c_str(), node.second->type_.c_str(), node.second->id_.c_str());
                    ready = false;
                }
            }
            if (ready) {
                MA_LOGI(TAG, "start node: %s(%s)", node.second->type_.c_str(), node.second->id_.c_str());
                node.second->onStart();
            }
        }
    }
    return n;
}

void NodeFactory::destroy(const std::string id) {
    Guard guard(m_mutex);

    // find node
    auto node = m_nodes.find(id);
    if (node == m_nodes.end()) {
        return;
    }

    for (auto& dep : node->second->dependents_) {
        if (find(dep.first)) {
            dep.second->onStop();
        }
    }

    // stop this node
    node->second->onStop();

    // call onDestroy
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
        destroy(node.first);
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
