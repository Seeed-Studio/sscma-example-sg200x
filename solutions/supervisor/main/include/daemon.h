#ifndef _DAEMON_H_
#define _DAEMON_H_

#include <thread>

enum APP_STATUS {
    APP_STATUS_NORMAL,
    APP_STATUS_STOP,
    APP_STATUS_NORESPONSE,
    APP_STATUS_UNKNOWN
};

extern int noderedStarting;
extern APP_STATUS noderedStatus;
extern APP_STATUS sscmaStatus;

void initDaemon();
void stopDaemon();

#endif
