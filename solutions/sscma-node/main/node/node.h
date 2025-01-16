#pragma once

#include <algorithm>
#include <functional>
#include <string>
#include <unordered_map>

#include "core/ma_core.h"
#include "porting/ma_porting.h"

namespace ma::node {

class NodeFactory;
class NodeServer;

class Node {
public:
    Node(std::string type, std::string id);
    virtual ~Node();

    virtual ma_err_t onCreate(const json& config)                            = 0;
    virtual ma_err_t onStart()                                               = 0;
    virtual ma_err_t onControl(const std::string& control, const json& data) = 0;
    virtual ma_err_t onStop()                                                = 0;
    virtual ma_err_t onDestroy()                                             = 0;

    const std::string& id() const;
    const std::string& type() const;
    const std::string dump() const;

protected:
    Mutex mutex_;
    std::string id_;
    std::string type_;
    std::atomic<bool> started_;
    std::atomic<bool> created_;
    std::atomic<bool> enabled_;
    std::unordered_map<std::string, Node*> dependencies_;
    std::unordered_map<std::string, Node*> dependents_;

    NodeServer* server_;

    friend class NodeServer;
    friend class NodeFactory;
};

class NodeFactory {

    using CreateNode = std::function<Node*(const std::string&)>;

    struct NodeCreator {
        CreateNode create;
        bool singleton;
    };

public:
    static Node* create(const std::string id, const std::string type, const json& data, NodeServer* server = nullptr);
    static void destroy(const std::string id);
    static Node* find(const std::string id);
    static void clear();

    static void registerNode(const std::string type, CreateNode create, bool singleton = false);

private:
    static std::unordered_map<std::string, NodeCreator>& registry();
    static std::unordered_map<std::string, Node*> m_nodes;
    static Mutex m_mutex;
};

#if MA_USE_NODE_REGISTRAR
#define REGISTER_NODE(type, node)                                                                \
    class node##Registrar {                                                                      \
    public:                                                                                      \
        node##Registrar() {                                                                      \
            NodeFactory::registerNode(type, [](const std::string& id) { return new node(id); }); \
        }                                                                                        \
    };                                                                                           \
    static node##Registrar g_##node##Registrar MA_ATTR_USED;

#define REGISTER_NODE_SINGLETON(type, node)                                                            \
    class node##Registrar {                                                                            \
    public:                                                                                            \
        node##Registrar() {                                                                            \
            NodeFactory::registerNode(type, [](const std::string& id) { return new node(id); }, true); \
        }                                                                                              \
    };                                                                                                 \
    static node##Registrar g_##node##Registrar MA_ATTR_USED;
#else
#define REGISTER_NODE(type, node)
#define REGISTER_NODE_SINGLETON(type, node)
#endif


}  // namespace ma::node
