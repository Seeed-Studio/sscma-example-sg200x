#ifndef _COMMON_H_
#define _COMMON_H_

// std::string getWifiConnectStatus();
// bool isLegalWifiIp();

// std::string getGateWay(std::string ip);
// std::string getIpAddress(const char* ifrName);
// std::string getNetmaskAddress(const char* ifrName);

#define CMD_BUF_SIZE 256
int exec_cmd(const char* cmd, char* result, const char* param);

#endif
