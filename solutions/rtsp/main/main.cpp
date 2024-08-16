#include <thread>

#include "BasicUsageEnvironment.hh"
#include "H264LiveVideoServerMediaSubsession.hh"
#include "liveMedia.hh"

#include "announceURL.hh"

UsageEnvironment* env;
H264LiveVideoServerMediaSubsession* session = NULL;

// To make the second and subsequent client for each stream reuse the same
// input stream as the first client (rather than playing the file from the
// start for each client), change the following "False" to "True":
Boolean reuseFirstSource = False;

static void announceStream(RTSPServer* rtspServer,
                           ServerMediaSession* sms,
                           char const* streamName,
                           char const* inputFileName);  // forward

void initRtsp() {
    // Begin by setting up our usage environment:
    TaskScheduler* scheduler = BasicTaskScheduler::createNew();
    env                      = BasicUsageEnvironment::createNew(*scheduler);

    RTSPServer* rtspServer = RTSPServer::createNew(*env, 8555);
    if (rtspServer == NULL) {
        *env << "Failed to create RTSP server: " << env->getResultMsg() << "\n";
        exit(1);
    }

    char const* descriptionString = "Session streamed by \"rtspServer\"";

    OutPacketBuffer::maxSize = 2000000;
    {
        char const* streamName = "live";
        ServerMediaSession* sms =
            ServerMediaSession::createNew(*env, streamName, streamName, descriptionString);
        session = H264LiveVideoServerMediaSubsession::createNew(*env, reuseFirstSource);
        sms->addSubsession(session);
        rtspServer->addServerMediaSession(sms);

        rtspServer->envir() << "\n\"" << streamName << "\" stream, read frames from a file\n";
        announceURL(rtspServer, sms);
    }

    {
        char const* streamName    = "h264";
        char const* inputFileName = "test.264";
        ServerMediaSession* sms =
            ServerMediaSession::createNew(*env, streamName, streamName, descriptionString);
        sms->addSubsession(
            H264VideoFileServerMediaSubsession::createNew(*env, inputFileName, reuseFirstSource));
        rtspServer->addServerMediaSession(sms);

        announceStream(rtspServer, sms, streamName, inputFileName);
    }

    env->taskScheduler().doEventLoop();  // does not return
}

// 150000
int read_one_frame(FILE* fp, uint8_t* ptr) {
    int size             = 0;
    static uint8_t hd[4] = {0};
    // printf("read one frame\n");

    if (fread(ptr, 1, 4, fp) < 4) {
        printf("read start error\n");
        return size;
    }

    if (ptr[0] == 0x00 && ptr[1] == 0x00 && ptr[2] == 0x00 && ptr[3] == 0x01) {
        size = 4;
        while (1) {
            if (fread(ptr + size, 1, 1, fp)) {
                hd[0] = hd[1];
                hd[1] = hd[2];
                hd[2] = hd[3];
                hd[3] = ptr[size];
                size++;
                if (hd[0] == 0x00 && hd[1] == 0x00 && hd[2] == 0x00 && hd[3] == 0x01) {
                    size -= 4;
                    fseek(fp, -4, SEEK_CUR);
                    break;
                }
            } else {
                break;
            }
        }
    }

    return size;
}

int main(int argc, char** argv) {
    const int MAX_SIZE = 150000;

    std::thread th(initRtsp);

    const char* fileName = "test.264";
    FILE* fp             = fopen(fileName, "rb");
    int fileSize = 0, curSize = 0;

    fseek(fp, 0L, SEEK_END);
    fileSize = ftell(fp);
    fseek(fp, 0L, SEEK_SET);
    printf("%s file size: %d\n", fileName, fileSize);

    int bufSize  = 0;
    uint8_t* buf = (uint8_t*)malloc(MAX_SIZE);

    while (1) {
        if (session == NULL || !session->isStart()) {
            printf("not start now\n");
            sleep(1);
            continue;
        }

        // fprintf(stderr, "this is main function\n");
        if (curSize < fileSize) {
            int tmp = read_one_frame(fp, buf);
            usleep(10 * 1000);
            if (tmp) {
                curSize += tmp;
                session->writeData(buf, tmp);
            }
        } else {
            curSize = 0;
            fseek(fp, 0L, SEEK_SET);
            printf("restart to read file[%d - %d]\n", curSize, fileSize);
        }
        // usleep(300);
        // sleep(1);
    }

    return 0;
}

static void announceStream(RTSPServer* rtspServer,
                           ServerMediaSession* sms,
                           char const* streamName,
                           char const* inputFileName) {
    UsageEnvironment& env = rtspServer->envir();

    env << "\n\"" << streamName << "\" stream, from the file \"" << inputFileName << "\"\n";
    announceURL(rtspServer, sms);
}
