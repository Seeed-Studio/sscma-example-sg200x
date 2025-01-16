#include "node.h"
#include "server.h"

namespace ma::node {

static constexpr char TAG[] = "ma::node";

Node::Node(std::string type, std::string id) : mutex_(), id_(std::move(id)), type_(std::move(type)), started_(false), enabled_(true), dependencies_(), dependents_(), server_(nullptr) {}

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
        MA_THROW(Exception(MA_EEXIST, "Node already exists: " + id));
        return nullptr;
    }

    // find node type
    auto it = registry().find(_type);
    if (it == registry().end()) {
        MA_THROW(Exception(MA_EINVAL, "Unknown node type: " + _type));
        return nullptr;
    }

    // check singleton
    if (it->second.singleton) {
        for (auto node : m_nodes) {
            if (node.second->type_ == _type) {
                MA_THROW(Exception(MA_EEXIST, "Singleton node already exists: " + id));
                return nullptr;
            }
        }
    }

    // create node
    MA_LOGI(TAG, "create node: %s(%s) %s", _type.c_str(), id.c_str(), data.dump().c_str());
    Node* n = it->second.create(id);
    if (!n) {
        MA_THROW(Exception(MA_ENOMEM, "Failed to create node: " + id));
        return nullptr;
    }
    n->server_ = server;
    if (MA_OK != n->onCreate(data["config"])) {
        return nullptr;
    }

    // set dependencies
    if (data.contains("dependencies")) {
        for (auto dep : data["dependencies"].get<std::vector<std::string>>()) {
            n->dependencies_[dep] = find(dep);
        }
    }

    // set dependents
    if (data.contains("dependents")) {
        for (auto dep : data["dependents"].get<std::vector<std::string>>()) {
            n->dependents_[dep] = find(dep);
        }
    }

    m_nodes[id] = n;

    // check dependencies
    for (auto node : m_nodes) {
        if (node.second->started_) {  // not started yet
            MA_LOGV(TAG, "skip check node: %s(%s) %s", node.second->type_.c_str(), node.second->id_.c_str(), id.c_str());
            continue;
        }
        bool ready = true;
        MA_LOGV(TAG, "check node: %s(%s) %s", node.second->type_.c_str(), node.second->id_.c_str(), id.c_str());
        for (auto& dep : node.second->dependencies_) {
            MA_LOGV(TAG, "dependencies: %s %p", dep.first.c_str(), dep.second);
            if (dep.second == nullptr && dep.first == id) {
                MA_LOGV(TAG, "dependencies %s ready: %s(%s)", dep.first.c_str(), node.second->type_.c_str(), node.second->id_.c_str());
                dep.second = n;
            }
            if (dep.second == nullptr) {
                MA_LOGV(TAG, "dependencies %s not ready: %s(%s)", dep.first.c_str(), node.second->type_.c_str(), node.second->id_.c_str());
                ready = false;
            }
        }
        for (auto& dep : node.second->dependents_) {
            MA_LOGV(TAG, "dependents: %s %p", dep.first.c_str(), dep.second);
            if (dep.second == nullptr && dep.first == id) {
                MA_LOGV(TAG, "dependents %s ready: %s(%s)", dep.first.c_str(), node.second->type_.c_str(), node.second->id_.c_str());
                dep.second = n;
            }
            if (dep.second == nullptr || !dep.second->started_) {
                MA_LOGV(TAG, "dependents %s not ready: %s(%s) ", dep.first.c_str(), node.second->type_.c_str(), node.second->id_.c_str());
                ready = false;
            }
        }
        if (ready) {
            MA_LOGI(TAG, "start node: %s(%s)", node.second->type_.c_str(), node.second->id_.c_str());
            MA_TRY {
                node.second->onStart();
            }
            MA_CATCH(Exception & e) {
                MA_LOGE(TAG, "failed to start node: %s(%s) %s", node.second->type_.c_str(), node.second->id_.c_str(), e.what());
                Thread::sleep(Tick::fromMilliseconds(20));
                server->response(node.first, json::object({{"type", MA_MSG_TYPE_RESP}, {"name", "start"}, {"code", e.err()}, {"data", e.what()}}));
                continue;
            }
            // if dependency is not ready, check it
            for (auto& dep : node.second->dependencies_) {
                if (!dep.second->started_) {
                    ready = true;
                    MA_LOGV(TAG, "check node: %s(%s) %s", dep.second->type_.c_str(), dep.second->id_.c_str(), node.first.c_str());
                    for (auto& d : dep.second->dependents_) {
                        if (d.second == nullptr && d.first == node.second->id_) {
                            MA_LOGV(TAG, "dependencies %s ready: %s(%s)", d.first.c_str(), dep.second->type_.c_str(), dep.second->id_.c_str());
                            d.second = node.second;
                        }
                        if (d.second == nullptr || !d.second->started_) {
                            MA_LOGV(TAG, "dependencies %s not ready: %s(%s)", d.first.c_str(), dep.second->type_.c_str(), dep.second->id_.c_str());
                            ready = false;
                        }
                    }
                    for (auto& d : dep.second->dependencies_) {
                        if (d.second == nullptr) {
                            MA_LOGV(TAG, "dependencies %s not ready: %s(%s)", d.first.c_str(), dep.second->type_.c_str(), dep.second->id_.c_str());
                            ready = false;
                        }
                    }
                    if (ready) {
                        MA_LOGI(TAG, "start node: %s(%s)", dep.second->type_.c_str(), dep.second->id_.c_str());
                        MA_TRY {
                            dep.second->onStart();
                        }
                        MA_CATCH(Exception & e) {
                            MA_LOGE(TAG, "start node: %s(%s) failed: %s", dep.second->type_.c_str(), dep.second->id_.c_str(), e.what());
                            Thread::sleep(Tick::fromMilliseconds(20));
                            server->response(dep.first, json::object({{"type", MA_MSG_TYPE_RESP}, {"name", "start"}, {"code", e.err()}, {"data", e.what()}}));
                            continue;
                        }
                    }
                }
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

    MA_LOGI(TAG, "destroy node: %s(%s)", id.c_str(), m_nodes[id]->type_.c_str());

    for (auto& dep : node->second->dependents_) {
        if (find(dep.first)) {
            MA_LOGD(TAG, "stop node: %s(%s)", dep.first.c_str(), dep.second->type_.c_str());
            dep.second->onStop();
            MA_LOGD(TAG, "stop node: %s(%s) done", dep.first.c_str(), dep.second->type_.c_str());
        }
    }

    // stop this node
    MA_LOGD(TAG, "stop node: %s(%s)", node->first.c_str(), node->second->type_.c_str());
    node->second->onStop();
    MA_LOGD(TAG, "stop node: %s(%s) done", node->first.c_str(), node->second->type_.c_str());

    // call onDestroy
    node->second->onDestroy();

    delete node->second;
    m_nodes.erase(id);

    MA_LOGD(TAG, "destroy node: %s done", id.c_str());

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
    MA_LOGI(TAG, "clear nodes");
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
