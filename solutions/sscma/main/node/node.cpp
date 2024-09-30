#include "node.h"

namespace ma::node {

static constexpr char TAG[] = "ma::node";

Node::Node(std::string type, std::string id)
    : mutex_(), id_(std::move(id)), type_(std::move(type)), started_(false), dependencies_() {
    MA_LOGD(TAG, "create node: %s(%s) %p", type_.c_str(), id_.c_str(), &mutex_);
}

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


Node* NodeFactory::create(const std::string id, const std::string type, const json& data) {

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
    Node* n = it->second.create(id);
    if (!n) {
        MA_THROW(Exception(MA_ENOMEM, "failed to create node: " + _type));
        return nullptr;
    }

    if (MA_OK != n->onCreate(data)) {
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

    if (data.contains("dependents")) {
        for (auto dep : data["dependents"].get<std::vector<std::string>>()) {
            n->dependents_[dep] = find(dep);
            if (n->dependents_[dep] == nullptr) {
                ready = false;
            }
        }
    }

    if (ready) {  // all dependencies are ready
        n->onStart();
    }

    // set dependencies for other nodes
    for (auto node : m_nodes) {
        if (!node.second->started_) {  // not started yet
            ready = true;
            for (auto dep : node.second->dependencies_) {
                if (dep.second == nullptr && dep.first == id) {
                    dep.second = n;
                }
                if (dep.second == nullptr) {
                    ready = false;
                }
            }
            for (auto dep : node.second->dependents_) {
                if (dep.second == nullptr && dep.first == id) {
                    dep.second = n;
                }
                if (dep.second == nullptr) {
                    ready = false;
                }
            }
            if (ready) {
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

    // stop all node depends on this node
    for (auto node : m_nodes) {
        auto it = node.second->dependencies_.find(id);
        if (it != node.second->dependencies_.end()) {
            it->second->onStop();
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
