#!/bin/sh

case $1 in
getAddress)
    ip route get $2 | awk '{print $NF}'
    ;;

restartApp)
    pid=`pidof $2`

    if [ "$pid" ]; then
        kill $pid
    fi

    $2 >/dev/null 2>&1 &
    ;;

esac
