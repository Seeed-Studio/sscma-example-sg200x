#ifndef _APP_IPC_DEVICE_H_
#define _APP_IPC_DEVICE_H_

int queryDeviceInfo(HttpRequest* req, HttpResponse* resp);
int updateDeviceName(HttpRequest* req, HttpResponse* resp);

int updateChannel(HttpRequest* req, HttpResponse* resp);
int setPower(HttpRequest* req, HttpResponse* resp);
int updateSystem(HttpRequest* req, HttpResponse* resp);

int getUpdateProgress(HttpRequest* req, HttpResponse* resp);
int cancelUpdate(HttpRequest* req, HttpResponse* resp);


#endif
