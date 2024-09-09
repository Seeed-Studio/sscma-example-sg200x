#!/bin/sh

case $1 in
getAddress)
    ip route get $2 | awk '{print $NF}'
    ;;

installApp)
    if [ -f "$2" ]; then
        opkg info $2 >/dev/null 2>&1
        [ $? != 0 ] && echo "File format error" && exit 1

        opkg install $2 >/dev/null 2>$1
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

restartApp)
    pid=`pidof $2`

    if [ "$pid" ]; then
        kill $pid
    fi

    $2 >/dev/null 2>&1 &
    ;;
esac
