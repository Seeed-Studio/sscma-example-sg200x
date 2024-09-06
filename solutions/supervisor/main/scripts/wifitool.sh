#!/bin/sh

ifname=wlan0
IFS=""

scan_wifi() {
    wpa_cli -i wlan0 scan 1> /dev/null
    wpa_cli -i wlan0 scan_results | tail -n +2 | while read -r line
    do
        printf "%b\n" $line | awk '$5 != "" {print $5, $3, $4, $1}'
    done
}

list_wifi() {
    # wpa_cli -i wlan0 list_networks | tail -n +2 | awk '{print $1, $2, $4}'
    wpa_cli -i wlan0 list_networks | tail -n +2 | while read -r line
    do
        printf "%b\n" $line | awk '{print $1, $2, $4}'
    done
}

connect_wifi() {
    ssid="'"'"'$1'"'"'"
    passwd="'"'"'$2'"'"'"

    id=`wpa_cli -i wlan0 add_network`
    echo $id

    ret=`echo $ssid | xargs wpa_cli -i wlan0 set_network $id ssid`
    if [ $ret == "OK" ]; then
        ret=`echo $passwd | xargs wpa_cli -i wlan0 set_network $id psk`
        if [ $ret == "OK" ]; then
            wpa_cli -i wlan0 enable_network $id
            wpa_cli -i wlan0 select_network $id
        else
            echo "incorrect password"
            wpa_cli -i wlan0 remove_network $id 1> /dev/null
        fi
    fi
}

wifi_status() {
    wifiStatus=`wpa_cli -i wlan0 status`

    ssid=`echo $wifiStatus | tr ' ' '\n' | grep "^ssid"`
    key_mgmt=`echo $wifiStatus | tr ' ' '\n' | grep "^key_mgmt"`
    address=`echo $wifiStatus | tr ' ' '\n' | grep "^address"`
    ip_address=`echo $wifiStatus | tr ' ' '\n' | grep "^ip_address"`

    printf "%b" $ssid | awk -F= '{ if (length($0) == 0) print "-"; else print $2 }'
    echo $key_mgmt | awk -F= '{ if (length($0) == 0) print "-"; else print $2 }'
    echo $address | awk -F= '{ if (length($0) == 0) print "-"; else print $2 }'
    echo $ip_address | awk -F= '{ if (length($0) == 0) print "-"; else print $2 }'
}

case $1 in
start)
    if [ -z "$(ifconfig wlan1 2> /dev/null)" ]; then
        iw dev wlan0 interface add wlan1 type __ap;
    fi

    if [ "$2" ]; then
        sed -i "s/ssid=.*$/ssid=$2/g" /etc/hostapd_2g4.conf
    fi

    ifconfig wlan1 up
    wpa_supplicant -B -i wlan0 -c /etc/wpa_supplicant.conf
    hostapd -B /etc/hostapd_2g4.conf
    ttyd -p $3 sh > /dev/null 2>&1 &
    ;;

stop)
    kill -9 `pidof wpa_supplicant`
    kill -9 `pidof hostapd`
    kill -9 `pidof ttyd`
    ifconfig wlan1 down
    ;;

state)
    wpa_cli -i wlan0 status | grep "^wpa_state" | awk -F= '{print $2}'
    ;;

get_gateway)
    iproute | grep default | grep "$2" | awk '{print $3}'
    ;;

scan)
    scan_wifi
    ;;

list)
    list_wifi
    ;;

connect)
    connect_wifi $2 $3
    wpa_cli -i wlan0 save_config
    ;;

select)
    echo $2
    wpa_cli -i wlan0 select_network $2
    ;;

disconnect)
    id=`wpa_cli -i wlan0 status | grep "^id" | awk -F= '{print $2}'`
    wpa_cli -i wlan0 disable_network $id
    wpa_cli -i wlan0 save_config
    ;;

status)
    wifi_status
    ;;

remove)
    wpa_cli -i wlan0 disable_network $2
    wpa_cli -i wlan0 remove_network $2
    wpa_cli -i wlan0 save_config
    ;;
esac
