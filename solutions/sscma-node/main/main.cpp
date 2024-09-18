
#include <iostream>
#include <signal.h>
#include <syslog.h>
#include <unistd.h>

#include <sscma.h>

#include "node/camera.h"
#include "node/model.h"
#include "node/node.h"
#include "node/save.h"
#include "node/stream.h"

#include "node/server.h"


using namespace ma::node;

NodeServer server("recamera");

static void sigHandler(int sig) {
    MA_LOGW("sscma", "Caught signal %d", sig);
    deinitVideo();
    exit(0);
}

int main(int argc, char* argv[]) {

    signal(SIGINT, sigHandler);
    signal(SIGSEGV, sigHandler);
    signal(SIGABRT, sigHandler);
    signal(SIGTERM, sigHandler);
    signal(SIGHUP, sigHandler);
    signal(SIGQUIT, sigHandler);
    signal(SIGPIPE, SIG_IGN);

    NodeFactory::registerNode("camera", [](std::string id) { return new CameraNode(id); }, true);
    NodeFactory::registerNode("model", [](std::string id) { return new ModelNode(id); }, true);
    NodeFactory::registerNode("stream", [](std::string id) { return new StreamNode(id); });
    NodeFactory::registerNode("save", [](std::string id) { return new SaveNode(id); });

    server.start("localhost", 1883);

    while (true) {
        Thread::sleep(Tick::fromSeconds(1));
    }

    return 0;
}