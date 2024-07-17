#include <sscma.h>

#define TAG "main"

int main(int argc, char** argv) {

    MA_LOGD(TAG, "build %s %s", __DATE__, __TIME__);

    EncoderJSON codec;
    ATServer server(&codec);

    server.init();

    server.start();

    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(10));
    }

    return 0;
}