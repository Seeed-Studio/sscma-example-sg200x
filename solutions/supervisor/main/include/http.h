#ifndef _HTTP_H_
#define _HTTP_H_

#include "global_cfg.h"

extern "C" {

#define HTTPS_SUPPORT 0

int initHttpd();
int deinitHttpd();

int initWiFi();

} // extern "C" {

#endif
