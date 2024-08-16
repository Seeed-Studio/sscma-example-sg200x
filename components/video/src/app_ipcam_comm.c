#include "app_ipcam_comm.h"
#include <sys/time.h>

unsigned int GetCurTimeInMsec(void)
{
    struct timeval tv;

    if (gettimeofday(&tv, NULL) < 0)
    {
        return 0;
    }

    return tv.tv_sec * 1000 + tv.tv_usec/1000;
}

