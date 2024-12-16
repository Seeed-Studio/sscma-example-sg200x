#!/bin/sh

APP_TOOL="/mnt/system/usr/scripts/apptool.sh"
CTRL_FILE=/tmp/upgrade.ctrl
START_FILE=/tmp/upgrade.start
PERCENTAGE_FILE=/tmp/upgrade.percentage

function restartApp() {
    app="$1"

    if [ -f "$app" ]; then
        $app restart >/dev/null 2>&1
        if [ $? == 0 ]; then
            echo "Finished"
        else
            echo "Restart failed"
        fi
    else
        echo "File does not exist"
    fi
}

case $1 in
getSystemStatus)
    fw_printenv boot_rollback | awk -F'=' '{print $NF}'
    ;;

getSnCode)
    fw_printenv sn | awk -F'=' '{print $NF}'
    ;;

getUpdateStatus)
    is_stop=""
    if [ -f "$CTRL_FILE" ]; then
        is_stop="`cat $CTRL_FILE`"
    fi
    if [[ -f $START_FILE && "$is_stop" != "stop" ]]; then
        echo "YES"
    else
        percentage=""
        if [ -f "$PERCENTAGE_FILE" ]; then
            percentage=$(cat $PERCENTAGE_FILE)
        fi
        if [ "$percentage" = "100" ]; then
            echo "YES"
        else
            echo "NO"
        fi
    fi
    ;;

getAddress)
    ip route get $2 | awk '{print $NF}'
    ;;

installApp)
    if [ -f "$2" ]; then
        currentVersion="`opkg list-installed | awk -F' - ' '{print $2}' | head -1`"
        ipkFile=$(dirname $2)/output/app.ipk
        status="`$APP_TOOL $2 $3 $4`" # appFile appName appVersion

        if [ "$status" == "ERROR" ]; then
            echo "File conversion failed" && exit 1
        fi

        opkg info $ipkFile >/dev/null 2>&1
        [ $? != 0 ] && echo "File format error" && exit 1

        status="`opkg install $ipkFile 2>/dev/null | grep "install completed"`"
        if [ "$status" ]; then
            echo "Finished"
        else
            echo "Install failed"
        fi
    else
        echo "File does not exist"
    fi
    ;;

getAppInfo)
    opkg list-installed | awk -F' - ' '{print $1 "\n" $2}'
    ;;

getFileMd5)
    md5sum $2 | awk '{print $1}'
    ;;

restartNodered)
    nodered="/etc/init.d/S*node-red"

    restartApp $nodered
    ;;

restartSscma)
    sscma="/etc/init.d/S*sscma-node"

    restartApp $sscma
    ;;

esac
