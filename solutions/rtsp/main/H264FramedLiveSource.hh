#ifndef _H264_FRAMED_LIVE_SOURCE_HH
#define _H264_FRAMED_LIVE_SOURCE_HH

#include <semaphore.h>
#include <time.h>
#include "UsageEnvironment.hh"
#include "FramedSource.hh"

class H264FramedLiveSource: public FramedSource {
public:
    static H264FramedLiveSource* createNew(UsageEnvironment& env);
    virtual ~H264FramedLiveSource();
    EventTriggerId getEventTriggerId() const;
    void setBuf(u_int8_t* buf, unsigned size);
    unsigned getBufSize();
    void afterDeliver();

protected:
    H264FramedLiveSource(UsageEnvironment& env);

private:
    virtual void doGetNextFrame();

private:
    static void deliverFrame0(void* clientData);
    void deliverFrame();

private:
    sem_t _sem;
    EventTriggerId _eventTriggerId;
    u_int8_t* _buf;
    unsigned _bufSize;
};

void SignalNewFrameData(TaskScheduler* task, H264FramedLiveSource* src);

#endif
