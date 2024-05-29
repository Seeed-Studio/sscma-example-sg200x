#ifndef _APP_IPC_WIFI_H_
#define _APP_IPC_WIFI_H_

int queryWiFiInfo(HttpRequest* req, HttpResponse* resp);
int scanWiFi(HttpRequest* req, HttpResponse* resp);
int connectWiFi(HttpRequest* req, HttpResponse* resp);
int disconnectWiFi(HttpRequest* req, HttpResponse* resp);
int switchWiFi(HttpRequest* req, HttpResponse* resp);
int getWifiStatus(HttpRequest* req, HttpResponse* resp);
int autoConnectWiFi(HttpRequest* req, HttpResponse* resp);
int forgetWiFi(HttpRequest* req, HttpResponse* resp);

#endif
