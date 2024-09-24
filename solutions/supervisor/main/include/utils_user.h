#ifndef _UTILS_USER_H_
#define _UTILS_USER_H_

extern int g_userId;

int initUserInfo();

int queryUserInfo(HttpRequest* req, HttpResponse* resp);
int updateUserName(HttpRequest* req, HttpResponse* resp);
int updatePassword(HttpRequest* req, HttpResponse* resp);
int addSShkey(HttpRequest* req, HttpResponse* resp);
int deleteSShkey(HttpRequest* req, HttpResponse* resp);

#endif
