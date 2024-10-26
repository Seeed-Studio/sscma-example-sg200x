#ifndef _UTILS_WIFI_H_
#define _UTILS_WIFI_H_

extern bool g_wifiStatus;

int queryWiFiInfo(HttpRequest* req, HttpResponse* resp);
int scanWiFi(HttpRequest* req, HttpResponse* resp);
int getWiFiScanResults(HttpRequest* req, HttpResponse* resp);
int connectWiFi(HttpRequest* req, HttpResponse* resp);
int disconnectWiFi(HttpRequest* req, HttpResponse* resp);
int switchWiFi(HttpRequest* req, HttpResponse* resp);
int getWifiStatus(HttpRequest* req, HttpResponse* resp);
int autoConnectWiFi(HttpRequest* req, HttpResponse* resp);
int forgetWiFi(HttpRequest* req, HttpResponse* resp);

std::string getWiFiName(const char* ifrName);
void monitorWifiStatusThread();

#endif
