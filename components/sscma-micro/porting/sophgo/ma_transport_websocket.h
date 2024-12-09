#ifndef _MA_TRANSPORT_WEBSOCKET_H
#define _MA_TRANSPORT_WEBSOCKET_H

#include <forward_list>

#include "core/ma_common.h"
#include "core/utils/ma_ringbuffer.hpp"
#include "porting/ma_osal.h"
#include "porting/ma_transport.h"

#include "hv/WebSocketServer.h"

namespace ma {

using json = hv::Json;

class TransportWebSocket final : public Transport {

public:
    struct Config {
        int port;
        std::string session;
    };


    TransportWebSocket();
    ~TransportWebSocket();

    ma_err_t init(const void* config) noexcept override;
    void deInit() noexcept override;

    size_t available() const noexcept override;
    size_t send(const char* data, size_t length) noexcept override;
    size_t receive(char* data, size_t length) noexcept override;
    size_t receiveIf(char* data, size_t length, char delimiter) noexcept override;
    size_t flush() noexcept override;

private:
    int m_port;
    Mutex m_mutex;
    hv::WebSocketService m_service;
    hv::WebSocketServer m_server;
    std::forward_list<WebSocketChannelPtr> m_channels;
    SPSCRingBuffer<char>* m_receiveBuffer;
    Thread* m_thread;
};

}  // namespace ma

#endif