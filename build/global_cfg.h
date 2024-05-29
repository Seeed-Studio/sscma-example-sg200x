#ifndef __COMMON_H__
#define __COMMON_H__

#ifdef __cplusplus
#define EXTERN_C_START() extern "C" {
#define EXTERN_C_END() }
#else
#define EXTERN_C_START()
#define EXTERN_C_END()
#endif

/* HTTPD */
#define HTTPD_PORT 80
#define WWW(file) ("/mnt/www/" file)

/* WebSocket */
#define WS_PORT 8000

#endif