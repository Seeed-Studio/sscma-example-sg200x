#ifndef _GLOBAL_CFG_H_
#define _GLOBAL_CFG_H_

/* HTTPD */
#define HTTPD_PORT 80
#define HTTPS_PORT 443
#define TTYD_PORT 9090
#define WWW(file) ("/mnt/system/usr/dist/" file)
#define REDIRECT_URL "http://192.168.16.1/index.html"
#define DEFAULT_UPGRADE_URL "https://github.com/Seeed-Studio/reCamera-OS/releases/latest"

/* WebSocket */
#define WS_PORT 8000

#define PATH_MODEL "/proc/device-tree/model"
#define PATH_ISSUE "/etc/issue"
#define PATH_SECRET "/etc/secret"
#define PATH_DEVICE_NAME "/etc/device-name"
#define PATH_UPGRADE_URL "/etc/upgrade"
#define PATH_SSH_KEY_FILE "/home/recamera/.ssh/authorized_keys"
#define PATH_TMP_KEY_FILE "/tmp/sshkey.tmp"
#define PATH_UPGRADE_PROGRESS_FILE "/tmp/upgrade.percentage"
#define PATH_UPGRADE_VERSION_FILE "/tmp/upgrade.version"
#define PATH_APP_DOWNLOAD_DIR "/tmp/opkg/"
#define PATH_MODEL_DOWNLOAD_DIR "/userdata/MODEL/"
#define PATH_APP_DOWNLOAD_DIR "/userdata/app/"

#define PATH_SERVER_CRT "/etc/cert/server.crt"
#define PATH_SERVER_KEY "/etc/cert/server.key"

/* usertool.sh*/
#define SCRIPT_USER(action) ("/mnt/system/usr/scripts/usertool.sh" \
                             " " #action " ")
#define SCRIPT_USER_ID SCRIPT_USER(id)
#define SCRIPT_USER_NAME SCRIPT_USER(name)
#define SCRIPT_USER_VERIFY SCRIPT_USER(verify)
#define SCRIPT_USER_PWD SCRIPT_USER(passwd)
#define SCRIPT_USER_SAVE SCRIPT_USER(save)
#define SCRIPT_USER_SSH SCRIPT_USER(query_key)
#define SCRIPT_USER_VERIFY_SSH SCRIPT_USER(verify_key)

/* wifitool.sh*/
#define SCRIPT_WIFI(action) ("/mnt/system/usr/scripts/wifitool.sh" \
                             " " #action " ")
#define SCRIPT_WIFI_START SCRIPT_WIFI(start)
#define SCRIPT_WIFI_STOP SCRIPT_WIFI(stop)
#define SCRIPT_WIFI_SCAN SCRIPT_WIFI(scan)
#define SCRIPT_WIFI_LIST SCRIPT_WIFI(list)
#define SCRIPT_WIFI_CONNECT SCRIPT_WIFI(connect)
#define SCRIPT_WIFI_CONNECT_STATUS SCRIPT_WIFI(connect_status)
#define SCRIPT_WIFI_GET_WIFI_ID SCRIPT_WIFI(get_wifi_id)
#define SCRIPT_WIFI_SELECT SCRIPT_WIFI(select)
#define SCRIPT_WIFI_DISCONNECT SCRIPT_WIFI(disconnect)
#define SCRIPT_WIFI_STATUS SCRIPT_WIFI(status)
#define SCRIPT_WIFI_REMOVE SCRIPT_WIFI(remove)
#define SCRIPT_WIFI_STATE SCRIPT_WIFI(state)
#define SCRIPT_WIFI_GATEWAY SCRIPT_WIFI(get_gateway)

/* upgrade.sh */
#define SCRIPT_UPGRADE(action) ("/mnt/system/usr/scripts/upgrade.sh" \
                                " " #action " ")
#define SCRIPT_UPGRADE_LATEST SCRIPT_UPGRADE(latest)
#define SCRIPT_UPGRADE_START SCRIPT_UPGRADE(start)
#define SCRIPT_UPGRADE_QUERY SCRIPT_UPGRADE(query)
#define SCRIPT_UPGRADE_STOP SCRIPT_UPGRADE(stop)

/* devicetool.sh */
#define SCRIPT_DEVICE(action) ("/mnt/system/usr/scripts/devicetool.sh" \
                                " " #action " ")
#define SCRIPT_DEVICE_GETSYSTEMSTATUS SCRIPT_DEVICE(getSystemStatus)
#define SCRIPT_DEVICE_GETSNCODE SCRIPT_DEVICE(getSnCode)
#define SCRIPT_DEVICE_GETADDRESSS SCRIPT_DEVICE(getAddress)
#define SCRIPT_DEVICE_INSTALLAPP SCRIPT_DEVICE(installApp)
#define SCRIPT_DEVICE_GETAPPINFO SCRIPT_DEVICE(getAppInfo)
#define SCRIPT_DEVICE_GETFILEMD5 SCRIPT_DEVICE(getFileMd5)
#define SCRIPT_DEVICE_RESTARTNODERED SCRIPT_DEVICE(restartNodered)
#define SCRIPT_DEVICE_RESTARTSSCMA SCRIPT_DEVICE(restartSscma)

#endif
