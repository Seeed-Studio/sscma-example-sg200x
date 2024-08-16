#include "H264LiveVideoServerMediaSubsession.hh"
// #include "H264VideoStreamFramer.hh"

#include <stdio.h>

H264LiveVideoServerMediaSubsession* H264LiveVideoServerMediaSubsession::createNew(UsageEnvironment& env, Boolean reuseFirstSource) {
    return new H264LiveVideoServerMediaSubsession(env, reuseFirstSource);
}

H264LiveVideoServerMediaSubsession::H264LiveVideoServerMediaSubsession(UsageEnvironment& env, Boolean reuseFirstSource)
    : OnDemandServerMediaSubsession(env, reuseFirstSource) {
    printf("[chenl] (H264LiveVideoServerMediaSubsession::%s) init done here\n", __func__);
    _start = false;
}

H264LiveVideoServerMediaSubsession::~H264LiveVideoServerMediaSubsession() {
}

void H264LiveVideoServerMediaSubsession::startStream(unsigned clientSessionId, void* streamToken,
        TaskFunc* rtcpRRHandler,
        void* rtcpRRHandlerClientData,
        unsigned short& rtpSeqNum,
        unsigned& rtpTimestamp,
        ServerRequestAlternativeByteHandler* serverRequestAlternativeByteHandler,
        void* serverRequestAlternativeByteHandlerClientData) {
    OnDemandServerMediaSubsession::startStream(
            clientSessionId, streamToken, rtcpRRHandler,
            rtcpRRHandlerClientData, rtpSeqNum, rtpTimestamp, serverRequestAlternativeByteHandler, serverRequestAlternativeByteHandlerClientData);

    _start = true;
    printf("[chenl] - start to get stream\n");
}

FramedSource* H264LiveVideoServerMediaSubsession::createNewStreamSource(unsigned clientSessionId, unsigned& estBitrate) {
    printf("[chenl] (H264LiveVideoServerMediaSubsession::%s) init done here\n", __func__);
    _liveSource = H264FramedLiveSource::createNew(envir());
    if (_liveSource == NULL) {
        return NULL;
    }

    // return H264VideoStreamFramer::createNew(envir(), liveSource);
    return H264VideoStreamDiscreteFramer::createNew(envir(), _liveSource); // no HAL header
}

RTPSink* H264LiveVideoServerMediaSubsession::createNewRTPSink(Groupsock* rtpGroupsock,
        unsigned char rtpPayloadTypeIfDynamic, FramedSource* inputSource) {
    printf("[chenl] (H264LiveVideoServerMediaSubsession::%s) init done here\n", __func__);
    return H264VideoRTPSink::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic);
}

bool H264LiveVideoServerMediaSubsession::isStart() {
    return _start;
}

int H264LiveVideoServerMediaSubsession::writeData(uint8_t* data, uint32_t size) {
#if 0
    printf("[chenl] (H264LiveVideoServerMediaSubsession::%s)::%d - size: %d, data: \n", __func__, __LINE__, size);
    for (int i = 0; i < size; i++) {
        printf("%02x ", data[i]);
    }
    printf("\n");
    // return 1;
#endif

    if (!_start) {
        return 1;
    }

    std::lock_guard<std::mutex> lock(_mutex);
    _liveSource->setBuf(data, size);

    while (_liveSource->getBufSize()) {
        // printf("[chenl] (H264LiveVideoServerMediaSubsession::%s)::%d - start to signal new frame data[%d]\n", __func__, __LINE__, size);
        SignalNewFrameData(&(envir().taskScheduler()), _liveSource);
        _liveSource->afterDeliver();
    }

    return 1;
}
