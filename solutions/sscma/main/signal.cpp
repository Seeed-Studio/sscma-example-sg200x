#include "signal.h"

std::unordered_map<int, std::vector<std::function<void(int)>>> Signal::m_callbacks;
std::mutex Signal::m_mutex;

void Signal::install(std::vector<int> sigs, std::function<void(int)> callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto sig : sigs) {
        if (m_callbacks.find(sig) == m_callbacks.end()) {
            signal(sig, Signal::sigHandler);
        }
        m_callbacks[sig].push_back(callback);
    }
}

void Signal::sigHandler(int sig) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_callbacks.find(sig);
    if (it != m_callbacks.end()) {
        for (auto& cb : it->second) {
            cb(sig);
        }
    }
}
