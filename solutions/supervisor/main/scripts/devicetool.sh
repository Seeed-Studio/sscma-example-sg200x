#!/bin/sh

case $1 in
getAddress)
    ip route get $2 | awk '{print $NF}'
    ;;

esac
