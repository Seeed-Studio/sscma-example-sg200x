#pragma once

#include <csignal>
#include <functional>
#include <mutex>
#include <unordered_map>
#include <vector>

class Signal {
public:
    static void install(std::vector<int> sigs, std::function<void(int sig)> callback);

private:
    static void sigHandler(int sig);

private:
    static std::unordered_map<int, std::vector<std::function<void(int sig)>>> m_callbacks;
    static std::mutex m_mutex;
};
