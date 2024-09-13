#!/bin/sh

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

getAddress)
    ip route get $2 | awk '{print $NF}'
    ;;

installApp)
    if [ -f "$2" ]; then
        opkg info $2 >/dev/null 2>&1
        [ $? != 0 ] && echo "File format error" && exit 1

        opkg install $2 >/dev/null 2>&1
        if [ $? == 0 ]; then
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

restartNodered)
    nodered="/etc/init.d/S92node-red"

    restartApp $nodered
    ;;

restartSscma)
    sscma="/etc/init.d/S91sscma-node"

    restartApp $sscma
    ;;

esac
