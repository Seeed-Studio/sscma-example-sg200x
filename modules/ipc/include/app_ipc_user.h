#ifndef _APP_IPC_USER_H_
#define _APP_IPC_USER_H_

int queryUserInfo(HttpRequest* req, HttpResponse* resp);
int updateUserName(HttpRequest* req, HttpResponse* resp);
int updatePassword(HttpRequest* req, HttpResponse* resp);
int addSShkey(HttpRequest* req, HttpResponse* resp);
int deleteSShkey(HttpRequest* req, HttpResponse* resp);

#endif
