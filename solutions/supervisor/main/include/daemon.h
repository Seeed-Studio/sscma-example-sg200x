#ifndef _DAEMON_H_
#define _DAEMON_H_

#include <thread>

enum APP_STATUS {
    APP_STATUS_NORMAL,
    APP_STATUS_NORESPONSE,
    APP_STATUS_UNKNOWN
};

void initDaemon();
void stopDaemon();

#endif
