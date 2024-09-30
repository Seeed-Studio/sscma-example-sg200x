#include <iostream>

#include <sscma.h>

int main(int argc, char** argv) {
    Device* device = Device::getInstance();

    static char buf[1024] = {0};
    while (true) {
        for (auto& transport : device->getTransports()) {
            if (int r_len = transport->available()) {
                transport->receive(buf, r_len > sizeof(buf) ? sizeof(buf) : r_len);
                std::cout << "recv: " << buf << std::endl;
                transport->send(buf, r_len);
            }
        }
    }

    return 0;
}