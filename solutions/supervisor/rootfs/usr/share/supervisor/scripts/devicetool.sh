#!/bin/sh

# APP_TOOL="/mnt/system/usr/scripts/apptool.sh"
# CTRL_FILE=/tmp/upgrade.ctrl
# START_FILE=/tmp/upgrade.start
# PERCENTAGE_FILE=/tmp/upgrade.percentage

# function restartApp() {
#     app="$1"

#     if [ -f "$app" ]; then
#         $app restart >/dev/null 2>&1
#         if [ $? == 0 ]; then
#             echo "Finished"
#         else
#             echo "Restart failed"
#         fi
#     else
#         echo "File does not exist"
#     fi
# }

FILE_ISSUE=/etc/issue

DIR_TMP=/tmp/supervisor
DIR_CONF=/etc/supervisor
FILE_DEV_NAME=device-name
FILE_SECRET=secret
FILE_UPGRADE=upgrade

mkdir -p $DIR_TMP
mkdir -p $DIR_CONF
if [ -f "/etc/$FILE_DEV_NAME" ]; then
    mv -f /etc/$FILE_DEV_NAME $DIR_CONF/$FILE_DEV_NAME
fi
if [ -f "/etc/$FILE_SECRET" ]; then
    mv -f /etc/$FILE_SECRET $DIR_CONF/$FILE_SECRET
fi
if [ -f "/etc/$FILE_UPGRADE" ]; then
    mv -f /etc/$FILE_UPGRADE $DIR_CONF/$FILE_UPGRADE
fi

case $1 in
getSystemStatus)
    fw_printenv boot_rollback | awk -F'=' '{print $NF}'
    ;;

getSnCode)
    fw_printenv sn | awk -F'=' '{print $NF}'
    ;;

getDeviceName)
    if [ -f "$DIR_CONF/$FILE_DEV_NAME" ]; then
        cat $DIR_CONF/$FILE_DEV_NAME
    else
        echo "unknown"
    fi
    ;;

getOsInfo)
    if [ -f "$FILE_ISSUE" ]; then
        cat $FILE_ISSUE
    fi
    ;;

updateDeviceName)
    if [ -z "$2" ]; then
        exit 1
    fi

    echo $2 > $DIR_CONF/$FILE_DEV_NAME

    # node-red
    sed -i "s/\\(username: \\)\"[^\"]*\"/\\1\"${2}\"/" /home/recamera/.node-red/settings.js

    # avahi
    sed -i s/^host-name=.*/host-name=${2}/ /etc/avahi/avahi-daemon.conf
    /etc/init.d/S50avahi-daemon reload
    ;;

updateChannel)
    if [ -z "$2" ]; then
        exit 1
    fi

    if [ "$2" = "0" ]; then
        echo "0" > $DIR_CONF/$FILE_UPGRADE
    else
        echo "1,${3}" > $DIR_CONF/$FILE_UPGRADE
    fi
    ;;

setPower)
    if [ -z "$2" ]; then
        exit 1
    fi

    if [ "$2" = "poweroff" ]; then
        poweroff
    fi

    if [ "$2" = "reboot" ]; then
        reboot
    fi
    ;;

getDeviceList)
    if [ -z "$2" ]; then
        avahi_result=$(mktemp)
        avahi-browse -arpt > $avahi_result
        ls $avahi_result
    else
        rm -rf $2
    fi
    ;;

# getUpdateStatus)
#     is_stop=""
#     if [ -f "$CTRL_FILE" ]; then
#         is_stop="`cat $CTRL_FILE`"
#     fi
#     if [[ -f $START_FILE && "$is_stop" != "stop" ]]; then
#         echo "YES"
#     else
#         percentage=""
#         if [ -f "$PERCENTAGE_FILE" ]; then
#             percentage=$(cat $PERCENTAGE_FILE)
#         fi
#         if [ "$percentage" = "100" ]; then
#             echo "YES"
#         else
#             echo "NO"
#         fi
#     fi
#     ;;

getAddress)
    ip route get $2 | awk '{for(i=1;i<=NF;i++) if($i=="src") print $(i+1)}'
    ;;

# installApp)
#     if [ -f "$2" ]; then
#         currentVersion="`opkg list-installed | awk -F' - ' '{print $2}' | head -1`"
#         ipkFile=$(dirname $2)/output/app.ipk
#         status="`$APP_TOOL $2 $3 $4`" # appFile appName appVersion

#         if [ "$status" == "ERROR" ]; then
#             echo "File conversion failed" && exit 1
#         fi

#         opkg info $ipkFile >/dev/null 2>&1
#         [ $? != 0 ] && echo "File format error" && exit 1

#         status="`opkg install $ipkFile 2>/dev/null | grep "install completed"`"
#         if [ "$status" ]; then
#             echo "Finished"
#         else
#             echo "Install failed"
#         fi
#     else
#         echo "File does not exist"
#     fi
#     ;;

# getAppInfo)
#     opkg list-installed | awk -F' - ' '{print $1 "\n" $2}'
#     ;;

getFileMd5)
    md5sum $2 | awk '{print $1}'
    ;;

# restartNodered)
#     nodered="/etc/init.d/S*node-red"

#     restartApp $nodered
#     ;;

# restartSscma)
#     sscma="/etc/init.d/S*sscma-node"

#     restartApp $sscma
#     ;;

esac
