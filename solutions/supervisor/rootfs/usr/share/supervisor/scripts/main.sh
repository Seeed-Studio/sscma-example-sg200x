#!/bin/bash

readonly SCRIPTS_DIR=$(dirname $(readlink -f $0))
readonly STR_OK="OK"
readonly STR_FAILED="Failed"

# conf
readonly ISSUE_FILE="/etc/issue"
readonly HOSTNAME_FILE="/etc/hostname"
readonly USER_NAME="recamera"
readonly CONFIG_DIR="/etc/recamera.conf"
readonly CONF_UPGRADE="$CONFIG_DIR/upgrade"

# userdata
readonly USERDATA_DIR="/userdata"
readonly MODEL_DIR="$USERDATA_DIR/Models"
readonly MODELS_PRESET="/usr/share/supervisor/models"

# flow
_check_flow() {
    local src_flow="/usr/share/supervisor/flows.json"
    local src_flow_gimbal="/usr/share/supervisor/flows_gimbal.json"
    local dst_flow="/home/recamera/.node-red/flows.json"

    [[ ! -f "$dst_flow" ]] && {
        [ -n "$(ifconfig can0 2>/dev/null | grep "HWaddr")" ] && {
            [ -f "$src_flow_gimbal" ] && src_flow=$src_flow_gimbal
        }
        [ -f "$src_flow" ] && {
            cp -f "$src_flow" "$dst_flow"
        }
    }
    [[ -f "$dst_flow" ]] && {
        chown "$USER_NAME":"$USER_NAME" "$dst_flow"
    }
}

# work_dir
readonly WORK_DIR="/tmp/supervisor"
[[ ! -d "$WORK_DIR" ]] && {
    /usr/bin/ssh-keygen -A >/dev/null 2>&1 &

    mkdir -p "$WORK_DIR"
    chmod 400 -R "$WORK_DIR"

    # compatible
    [ ! -d "$CONFIG_DIR" ] && mkdir -p "$CONFIG_DIR"
    [ -f "/etc/upgrade" ] && mv -f /etc/upgrade "$CONF_UPGRADE"
    [ -s "/etc/device-name" ] && {
        mv -f "/etc/device-name" "$HOSTNAME_FILE"
        hostname -F "$HOSTNAME_FILE"
    }

    _check_flow >/dev/null &
}

_ip() { ifconfig "$1" 2>/dev/null | awk '/inet addr:/ {print $2}' | awk -F':' '{print $2}'; }
_mask() { ifconfig "$1" 2>/dev/null | awk '/Mask:/ {print $4}' | awk -F':' '{print $2}'; }
_mac() { ifconfig "$1" 2>/dev/null | awk '/HWaddr/ {print $5}'; }
_gateway() { route -n | grep '^0.0.0.0' | grep -w "$1" | awk '{print $2}'; }
_dns() { cat /etc/resolv.conf 2>/dev/null | awk '/nameserver/ {print $2}' | head -n 1; }
_dns2() { cat /etc/resolv.conf 2>/dev/null | awk '/nameserver/ {print $2}' | tail -n 1; }

_sn() { fw_printenv sn | awk -F'=' '{print $NF}'; }
_dev_name() { cat "$HOSTNAME_FILE"; }
_os_name() { cat "$ISSUE_FILE" 2>/dev/null | awk -F' ' '{print $1}'; }
_os_version() { cat "$ISSUE_FILE" 2>/dev/null | awk -F' ' '{print $2}'; }

_stop_pidname() {
    local sig=${2:-9}
    for pid in $(pidof "$1"); do [ -d "/proc/$pid" ] && kill -$sig $pid; done
}

##################################################
# file
api_file() { echo "$USERDATA_DIR"; }
# file
##################################################

##################################################
# device
readonly AVAHI_CONF="/etc/avahi/avahi-daemon.conf"
readonly AVAHI_SERVICE="/etc/init.d/S50avahi-daemon"

function getDeviceList() {
    local out="$WORK_DIR/${FUNCNAME[0]}"
    local tmp="$out.tmp"
    [ -s "$tmp" ] && { cp "$tmp" "$out"; }
    [ -z "$(pidof avahi-browse 2>/dev/null)" ] && { avahi-browse -arpt >"$tmp" 2>/dev/null & }
    [ -f "$out" ] || >"$out"
    echo "$out"
}

function updateDeviceName() {
    local dev_name=$2
    if [ -z $dev_name ]; then
        echo $STR_FAILED
        exit 1
    fi

    echo $dev_name >"$HOSTNAME_FILE"
    hostname -F "$HOSTNAME_FILE"
    sed -i s/^host-name=.*/host-name=$dev_name/ $AVAHI_CONF
    $AVAHI_SERVICE stop >/dev/null 2>&1
    sleep 0.05
    $AVAHI_SERVICE start >/dev/null 2>&1
    echo $STR_OK
}

function queryDeviceInfo() {
    local ch=$(cat $CONF_UPGRADE 2>/dev/null | awk -F',' '{print $1}')
    local url=$(cat $CONF_UPGRADE 2>/dev/null | awk -F',' '{print $2}')
    cat <<EOF
{
    "channel": "$((ch))",
    "serverUrl": "$url",
    "needRestart": $(_upgrade_done),
    "ethIp": "$(_ip "eth0")",
    "wlanIp": "$(_ip "wlan0")",
    "mask": "$(_mask "wlan0")",
    "mac": "$(_mac "wlan0")",
    "gateway": "$(_gateway "wlan0")",
    "dns": "$(_dns)",
    "deviceName": "$(cat $HOSTNAME_FILE)"
}
EOF
}

# model
readonly MODEL_FILE="$MODEL_DIR/model.cvimodel"
readonly MODEL_INFO="$MODEL_DIR/model.json"
_check_models() {
    [ ! -d "$MODEL_DIR" ] && { mkdir -p "$MODEL_DIR"; }
    [ ! -f "$MODEL_FILE" ] && {
        local src_model="$MODELS_PRESET/yolo11n_detection_cv181x_int8.cvimodel"
        local src_info="$MODELS_PRESET/yolo11n_detection_cv181x_int8.json"
        cp -f "$src_model" "$MODEL_FILE"
        cp -f "$src_info" "$MODEL_INFO"
        sync
    }
}

# upgrade
readonly DEFAULT_UPGRADE_URL="https://github.com/Seeed-Studio/reCamera-OS/releases/latest"
readonly UPGRADE="$SCRIPTS_DIR/upgrade.sh"
readonly UPGRADE_CANCEL="$WORK_DIR/upgrade.cancel"
readonly UPGRADE_DONE="$WORK_DIR/upgrade.done"
readonly UPGRADE_MUTEX="$WORK_DIR/upgrade.mutex"
readonly UPGRADE_PROG="$WORK_DIR/upgrade.prog"

_upgrading() { [ -f "$UPGRADE_PROG" ] && echo "1" || echo "0"; }
_upgrade_done() { [ -f "$UPGRADE_DONE" ] && echo "1" || echo "0"; }

function updateChannel() {
    if [ "$(_upgrading)" = "1" ]; then
        echo "It's upgrading now..."
        return 1
    fi

    local ch=$2 url=$3
    echo $ch,$url >$CONF_UPGRADE
    echo $STR_OK
    $UPGRADE clean >/dev/null 2>&1
}

function cancelUpdate() {
    echo 1 >"$UPGRADE_CANCEL"
    rm -rf $UPGRADE_PROG
    $UPGRADE download x >/dev/null 2>&1 &
    $UPGRADE start x >/dev/null 2>&1 &
    echo "$STR_OK"
}

_upgrade_latest() {
    local url
    local ch=$(cat $CONF_UPGRADE 2>/dev/null | awk -F'[, ]' '{print $1}')
    [ $((ch)) -ne 0 ] && {
        url=$(cat $CONF_UPGRADE 2>/dev/null | awk -F'[, ]' '{print $2}')
        [ -z "$url" ] && return
    }
    for i in {1..3}; do
        $UPGRADE latest $url >/dev/null 2>&1
        local out=$($UPGRADE latest q)
        [ "$(echo $out | cut -d':' -f1)" = "Success" ] && {
            break
        }
    done
}

function getSystemUpdateVersion() {
    local os ver
    local status=3 # querying
    if [[ "$(_upgrading)" = "1" ]]; then
        status=2
    else
        local out=$($UPGRADE latest q)
        if [ "$(echo $out | cut -d':' -f1)" = "Success" ]; then
            status=1
            out2=$(echo $out | cut -d':' -f2)
            os=$(echo $out2 | cut -d' ' -f1)
            ver=$(echo $out2 | cut -d' ' -f2)
        else
            [ -z "$(pidof $UPGRADE latest)" ] && { _upgrade_latest >/dev/null & }
        fi
    fi
    cat <<EOF
{ "osName": "$os", "osVersion": "$ver", "status": $status }
EOF
}

_upgrade_prog() {
    local mutex="$UPGRADE_MUTEX"
    [ -f "$mutex" ] && { return 0; }
    >$mutex
    local result size total prog1 prog2

    # download
    read -r result size total <<<"$($UPGRADE download q | awk -F'[: ]' '{print $1, $3, $4}')"
    if [[ "$result" == "Failed" || "$result" == "Stopped" ]]; then
        $UPGRADE download >/dev/null 2>&1 &
        printf '{"progress": "0", "status": "download"}'
        rm -f "$mutex"
        return 0
    elif [ "$result" != "Success" ]; then
        [ $((total)) -gt 0 ] && prog1=$((size * 100 / total))
        printf '{"progress": "%d", "status": "download"}' "$((prog1 / 2))"
        rm -f "$mutex"
        return 0
    fi
    prog1=100

    # upgrade
    read -r result size total <<<"$($UPGRADE start q | awk -F'[: ]' '{print $1, $3, $4}')"
    if [[ "$result" == "Failed" || "$result" == "Stopped" ]]; then
        $UPGRADE start >/dev/null 2>&1 &
        printf '{"progress": "50", "status": "upgrade"}'
        rm -f "$mutex"
        return 0
    fi
    if [ "$result" = "Success" ]; then
        prog2=100
    else
        [ $((total)) -gt 0 ] && prog2=$((size * 100 / total))
    fi
    total=$(((prog1 + prog2) / 2))
    printf '{"progress": "%d", "status": "upgrade"}' $total

    [ $total -ge 100 ] && echo 1 >"$UPGRADE_DONE"
    rm -f "$mutex"
}

function getUpdateProgress() {
    [ -f "$UPGRADE_CANCEL" ] && {
        echo "Cancelled"
        return 0
    }
    [ -s "$UPGRADE_PROG.tmp" ] && { cp $UPGRADE_PROG.tmp $UPGRADE_PROG; }
    if [ ! -s "$UPGRADE_PROG" ]; then
        echo '{"progress": "0", "status": ""}' >$UPGRADE_PROG
    fi
    cat $UPGRADE_PROG
    _upgrade_prog >$UPGRADE_PROG.tmp 2>/dev/null &
}

function updateSystem() {
    rm -f "$WORK_DIR/upgrade"*
    echo $STR_OK
}

function api_device() {
    _check_models >/dev/null &

    local rollback=0
    [ "$(fw_printenv boot_rollback)" = "boot_rollback=1" ] && rollback=1
    local wifi=0
    _check_wifi && wifi=1
    local canbus=0
    [ -n "$(ifconfig can0 2>/dev/null | grep "HWaddr")" ] && canbus=1
    local emmc=$(lsblk -b | grep -w mmcblk0 | awk '{print $4}')
    emmc=${emmc:-0}
    local sensor=0
    [[ -n "$(i2cdetect -y -r 2 36 36 | grep 36)" ]] && sensor=1
    [[ -n "$(i2cdetect -y -r 2 3f 3f | grep 3f)" ]] && sensor=2

    cat <<EOF
{
    "cpu": "sg2002", "ram": "256", "npu": "1", "ws": "8000", "ttyd": "9090",
    "emmc": ${emmc:-0},
    "wifi": $wifi,
    "canbus": $canbus,
    "sensor": $sensor,
    "rollback": $rollback,
    "sn": "$(_sn)",
    "dev_name": "$(_dev_name)",
    "os": "$(_os_name)",
    "ver": "$(_os_version)",
    "url": "$DEFAULT_UPGRADE_URL",
    "platform_info": "$CONFIG_DIR/platform.info",
    "model": {
        "dir": "$MODEL_DIR",
        "file": "$MODEL_FILE",
        "info": "$MODEL_INFO",
        "preset": "$MODELS_PRESET"
    }
}
EOF
}
# device
##################################################

##################################################
# user
readonly SSH_DIR="/home/$USER_NAME/.ssh"
readonly SSH_KEY_FILE="$SSH_DIR/authorized_keys"
readonly FIRST_LOGIN="/etc/.first_login"
readonly DIR_INID="/etc/init.d"
readonly DIR_INID_DISABLED="$DIR_INID/disabled"
readonly SSH_SERVICE="S50sshd"

# private
function _is_key_valid() {
    local tmp=$(mktemp)
    echo "$*" >$tmp
    ssh-keygen -lf $tmp 2>/dev/null
    local ret=$?
    rm $tmp
    return $ret
}

function _is_key_exist() {
    # Todo: check sha256
    grep -e "$*" $SSH_KEY_FILE >/dev/null 2>&1
    return $?
}

function _update_sshkey() {
    local sshKeyFile="$WORK_DIR/sshkey"
    >$sshKeyFile
    if [ -f $SSH_KEY_FILE ]; then
        local cnt=1
        while IFS= read -r line; do
            [[ "$line" =~ ^[0-9]+$ ]] && continue # skip comment line

            local sh256=$(_is_key_valid $line)
            if [ -n "$sh256" ]; then
                echo "$cnt $sh256" >>$sshKeyFile
                let cnt++
            fi
        done <$SSH_KEY_FILE
    fi
}

# APIs
function gen_token() { dd if=/dev/random bs=64 count=1 2>/dev/null | base64 -w0; }
function get_username() { echo $USER_NAME; }
function login() { rm -f "$FIRST_LOGIN"; }

function addSShkey() {
    local name="$2"
    local time="$3"
    shift 3
    local key="$*"

    _is_key_valid $key >/dev/null
    if [ $? -ne 0 ]; then
        echo "Invalid"
        exit 1
    fi

    local filename=$SSH_KEY_FILE
    if [ ! -f "$filename" ]; then
        mkdir -p $SSH_DIR
        touch $filename
        chown -R $USER_NAME:$USER_NAME $SSH_DIR
    fi

    _is_key_exist $key
    if [ 0 -eq $? ]; then
        echo "Already exist"
        exit 1
    fi

    echo "$key" " # " "$name $time" >>$filename
    echo $STR_OK
}

function deleteSShkey() {
    local line="$2"
    if ! [[ "$line" =~ ^[0-9]+$ ]]; then
        echo "Invalid id=$line"
        exit 1
    fi
    # Todo: skip comment line
    sed -i "${line}d" "$SSH_KEY_FILE"
    echo $STR_OK
}

_set_ssh_status() {
    local status=$1
    local dir="$DIR_INID"
    local dir_disabled="$DIR_INID_DISABLED"

    if [ $((status)) -eq 0 ]; then # disable
        path="$dir/$SSH_SERVICE"
        [[ -f "$path" ]] && {
            [[ ! -d "$dir_disabled" ]] && mkdir -p "$dir_disabled"
            $path stop
            mv "$path" "$dir_disabled"/
        }
    else
        path="$dir_disabled/$SSH_SERVICE"
        [[ -f "$path" ]] && {
            $path start
            mv "$path" "$dir"/
        }
    fi
}

function setSShStatus() {
    echo $STR_OK
    _set_ssh_status "$2" >/dev/null 2>&1 &
}

function queryUserInfo() {
    local firstLogin="false"
    local sshEnabled="false"
    local sshKeyFile="$WORK_DIR/sshkey"

    [[ -f "$FIRST_LOGIN" ]] && firstLogin="true"
    [[ -f "$DIR_INID/$SSH_SERVICE" ]] && sshEnabled="true"
    [[ -f "$sshKeyFile" ]] || >"$sshKeyFile"

    _update_sshkey
    cat <<EOF
{ "userName": "${USER_NAME}", "firstLogin": ${firstLogin}, 
"sshEnabled": ${sshEnabled}, "sshKeyFile": "${sshKeyFile}"
}
EOF
}

function updatePassword() {
    local pwd="$2"
    {
        echo "$pwd"
        echo "$pwd"
    } | passwd "$USER_NAME" >/dev/null 2>&1
    if [ $? -eq 0 ]; then
        echo "$STR_OK"
    else
        echo "$STR_FAILED"
    fi
}
# user
##################################################

##################################################
# network
readonly CONF_WIFI="$CONFIG_DIR/sta"
readonly CONF_AP="$CONFIG_DIR/ap"
readonly WPA_CLI="wpa_cli -i wlan0"

_check_wifi() { [ -z "$(ifconfig wlan0 2>/dev/null)" ] && return 1 || return 0; }
_sta_stop() { _stop_pidname "wpa_supplicant"; }
_ap_stop() {
    _stop_pidname "hostapd"
    ifconfig wlan1 0
    ifconfig wlan1 down
    sleep 0.5
    /etc/init.d/S80dnsmasq restart
    /etc/init.d/S49ntp restart
}

_sta_start() {
    _check_wifi || return 0
    [ -z "$(pidof wpa_supplicant 2>/dev/null)" ] && {
        ifconfig wlan0 down
        ifconfig wlan0 up
        wpa_supplicant -B -i wlan0 -c "/etc/wpa_supplicant.conf" >/dev/null 2>&1
    }
}

_ap_start() {
    _check_wifi || return 0
    local conf="/etc/hostapd_2g4.conf"
    local ssid=$(cat "$conf" | grep -w "ssid" | awk -F'=' '{print $2}')
    [ "$ssid" = "AUOK" ] && {
        local mac=$(_mac wlan1)
        ssid=$(echo $mac | awk -F':' '{print $4$5$6}')
        ssid="reCamera_$ssid"
        sed -i "s/ssid=.*$/ssid=$ssid/" "$conf"
        sync
    }
    [ -z "$(pidof hostapd 2>/dev/null)" ] && {
        ifconfig wlan1 down
        ifconfig wlan1 up
        hostapd -B "$conf" >/dev/null 2>&1
    }
}

function start_wifi() {
    local sta=-1 ap=-1
    _check_wifi && {
        [ ! -f "$CONF_WIFI" ] && echo 1 >"$CONF_WIFI"
        sta=$(cat "$CONF_WIFI")
        [ $((sta)) -eq 1 ] && { _sta_start >/dev/null 2>&1 & }

        [ ! -f "$CONF_AP" ] && echo 1 >"$CONF_AP"
        ap=$(cat "$CONF_AP")
        [ $((ap)) -eq 1 ] && { _ap_start >/dev/null 2>&1 & }
    }
    printf '{"sta": %d, "ap": %d}' $((sta)) $((ap))
}

function stop_wifi() {
    _sta_stop >/dev/null 2>&1 &
    _ap_stop >/dev/null 2>&1 &
}

function switchWiFi() {
    echo $2 >"$CONF_WIFI"
    [ "$2" = "0" ] && stop_wifi || start_wifi
}

function get_sta_connected() {
    local out=$WORK_DIR/${FUNCNAME[0]}
    >"$out"
    [ -z "$1" ] && { $WPA_CLI reconfigure >/dev/null 2>&1; }
    $WPA_CLI list_networks 2>/dev/null | while IFS= read -r line; do
        printf "%b\n" "$line" >>"$out"
    done
    echo "$out"
}

function get_sta_current() {
    local out=$WORK_DIR/${FUNCNAME[0]}
    $WPA_CLI status 2>/dev/null >"$out"
    echo "$out"
}

function get_eth() {
    local ifname="eth0"
    cat <<EOF
{
    "ip": "$(_ip "$ifname")",
    "subnetMask": "$(_mask "$ifname")",
    "macAddress": "$(_mac "$ifname")",
    "gateway": "$(_gateway "$ifname")",
    "dns1": "$(_dns "$ifname")",
    "dns2": "$(_dns2 "$ifname")"
}
EOF
}

function get_scan_list() {
    local out=$WORK_DIR/${FUNCNAME[0]}
    local result="$out.tmp"
    [ -s "$result" ] && {
        cp "$result" "$out"
        rm "$result"
    }
    [ -f "$out" ] || >"$out"
    echo "$out"
    [ -z "$(pidof iw 2>/dev/null)" ] && { iw dev wlan0 scan >"$result" 2>/dev/null & }
    return 0
}

function connectWiFi() {
    local id="$2" ssid="$3" pwd="$4"

    # Todo: filter duplicate ssid
    $WPA_CLI reconfigure >/dev/null 2>&1
    [ $((id)) -lt 0 ] && { # create new
        id=$($WPA_CLI add_network)

        local ret=$($WPA_CLI set_network "$id" ssid "\"$ssid\"")
        [ "$ret" != "OK" ] && {
            $WPA_CLI remove_network "$id" >/dev/null 2>&1
            return
        }

        if [ "$pwd" == "" ]; then
            $WPA_CLI set_network "$id" key_mgmt NONE >/dev/null 2>&1
        else
            ret=$($WPA_CLI set_network "$id" psk "\"$pwd\"")
            [ "$ret" != "OK" ] && {
                $WPA_CLI remove_network "$id" >/dev/null 2>&1
                return
            }
        fi
    }
    [ $((id)) -lt 0 ] && {
        echo "$STR_FAILED"
        return
    }

    local ids=$(cat $(get_sta_connected "0") | grep "^[0-9]" | awk -F'\t' '{print $1}')
    for i in $ids; do
        local pri=0
        [ $((i)) -eq $((id)) ] && { pri=100; }
        $WPA_CLI set_network "$i" priority "$pri" >/dev/null 2>&1
    done

    $WPA_CLI enable_network "$id" >/dev/null 2>&1
    $WPA_CLI save_config >/dev/null 2>&1
    $WPA_CLI disconnect >/dev/null 2>&1
    $WPA_CLI select_network "$id" >/dev/null 2>&1
    echo "$STR_OK"
}

_wpa_set_networks() {
    local ssid="$1" status="$2"
    local id=$(cat $(get_sta_connected) | grep -w "$ssid" | awk -F'\t' '{print $1}')
    [ -z "$id" ] && return 0
    $WPA_CLI "$status" "$id"
    $WPA_CLI save_config
}

function disconnectWiFi() {
    echo "$STR_OK"
    _wpa_set_networks "$2" disable_network >/dev/null 2>&1 &
}

function forgetWiFi() {
    echo "$STR_OK"
    _wpa_set_networks "$2" remove_network >/dev/null 2>&1 &
}
# network
##################################################

##################################################
# halow
readonly CONF_HALOW="$CONFIG_DIR/halow"
readonly WPA_CLI_S1G="wpa_cli_s1g -i halow0"

_check_halow() { [ -z "$(ifconfig halow0 2>/dev/null)" ] && return 1 || return 0; }
_halow_stop() { _stop_pidname "wpa_supplicant_s1g"; }

_halow_start() {
    _check_halow || return 0
    [ -z "$(pidof wpa_supplicant_s1g 2>/dev/null)" ] && {
        ifconfig halow0 down
        ifconfig halow0 up
        wpa_supplicant_s1g -B -Dnl80211 -ihalow0 -c "/etc/wpa_supplicant_s1g.conf" >/dev/null 2>&1
    }
}

function start_halow() {
    local halow=-1
    _check_halow && {
        [ ! -f "$CONF_HALOW" ] && echo 1 >"$CONF_HALOW"
        halow=$(cat "$CONF_HALOW")
        [ $((halow)) -eq 1 ] && { _halow_start >/dev/null 2>&1 & }
    }
    printf '{"halow": %d}' $((halow))
}

function stop_halow() {
    _halow_stop >/dev/null 2>&1 &
}

function switchHalow() {
    echo $2 >"$CONF_HALOW"
    [ "$2" = "0" ] && stop_halow || start_halow
}

function get_halow_current() {
    local out=$WORK_DIR/${FUNCNAME[0]}
    $WPA_CLI_S1G status 2>/dev/null > "$out"
    echo "$out"
}

function get_halow_connected() {
    local out=$WORK_DIR/${FUNCNAME[0]}
    >"$out"
    [ -z "$1" ] && { $WPA_CLI_S1G reconfigure >/dev/null 2>&1; }
    $WPA_CLI_S1G list_networks 2>/dev/null | while IFS= read -r line; do
        printf "%b\n" "$line" >>"$out"
    done
    echo "$out"
}

function get_halow_scan_list() {
    local out=$WORK_DIR/${FUNCNAME[0]}
    local result="$out.tmp"
    [ -s "$result" ] && {
        cp "$result" "$out"
        rm "$result"
    }
    [ -f "$out" ] || >"$out"
    echo "$out"
    $WPA_CLI_S1G scan_results >"$result" 2>/dev/null &
    return 0
}

function connectHalow() {
    local id="$2" ssid="$3" pwd="$4"

    # Todo: filter duplicate ssid
    $WPA_CLI_S1G reconfigure >/dev/null 2>&1
    [ $((id)) -lt 0 ] && { # create new
        id=$($WPA_CLI_S1G add_network)

        local ret=$($WPA_CLI_S1G set_network "$id" ssid "\"$ssid\"")
        [ "$ret" != "OK" ] && {
            $WPA_CLI_S1G remove_network "$id" >/dev/null 2>&1
            return
        }

        if [ "$pwd" == "" ]; then
            $WPA_CLI_S1G set_network "$id" key_mgmt NONE >/dev/null 2>&1
        else
            ret=$($WPA_CLI_S1G set_network "$id" psk "\"$pwd\"")
            [ "$ret" != "OK" ] && {
                $WPA_CLI_S1G remove_network "$id" >/dev/null 2>&1
                return
            }
            $WPA_CLI_S1G set_network "$id" key_mgmt SAE WPA-PSK >/dev/null 2>&1
            $WPA_CLI_S1G set_network "$id" ieee80211w 2 >/dev/null 2>&1
        fi
    }
    [ $((id)) -lt 0 ] && {
        echo "$STR_FAILED"
        return
    }

    local ids=$(cat $(get_halow_connected "0") | grep "^[0-9]" | awk -F'\t' '{print $1}')
    for i in $ids; do
        local pri=0
        [ $((i)) -eq $((id)) ] && { pri=100; }
        $WPA_CLI_S1G set_network "$i" priority "$pri" >/dev/null 2>&1
    done

    $WPA_CLI_S1G enable_network "$id" >/dev/null 2>&1
    $WPA_CLI_S1G save_config >/dev/null 2>&1
    $WPA_CLI_S1G disconnect >/dev/null 2>&1
    $WPA_CLI_S1G select_network "$id" >/dev/null 2>&1
    echo "$STR_OK"
}

_halow_set_networks() {
    local ssid="$1" status="$2"
    local id=$(cat $(get_halow_connected) | grep -w "$ssid" | awk -F'\t' '{print $1}')
    [ -z "$id" ] && return 0
    $WPA_CLI_S1G "$status" "$id"
    $WPA_CLI_S1G save_config
}

function disconnectHalow() {
    echo "$STR_OK"
    _halow_set_networks "$2" disable_network >/dev/null 2>&1 &
}

function forgetHalow() {
    echo "$STR_OK"
    _halow_set_networks "$2" remove_network >/dev/null 2>&1 &
}

# halow
##################################################

##################################################
# deamon
function query_sscma() {
    mosquitto_rr -h localhost -p 1883 -q 1 -v -W 3 \
        -t "sscma/v0/recamera/node/in/" \
        -e "sscma/v0/recamera/node/out/" \
        -m "{\"name\":\"health\",\"type\":3,\"data\":\"\"}" 2>/dev/null
    [ $? -ne 0 ] && { echo "$STR_FAILED"; }
    return 0
}

function query_nodered() {
    local result="$(curl -I --connect-timeout 2 --max-time 10 "localhost:1880" 2>/dev/null)"
    [ -z "$result" ] && {
        echo "$STR_FAILED"
        return
    }
    echo "$STR_OK"
}

function query_flow() {
    local result="$(curl --connect-timeout 2 --max-time 10 "localhost:1880/flows/state" 2>/dev/null)"
    [ -z "$result" ] && {
        echo "$STR_FAILED"
        return
    }
    echo "$result"
}

function ctrl_flow() {
    local state="$2"
    curl -s -X POST -H "Content-Type: application/json" -d "{\"state\":\"$state\"}" http://localhost:1880/flows/state
}

function start_service() {
    local service="$2"
    case "$service" in
    "sscma")
        /etc/init.d/S91sscma-node restart >/dev/null 2>&1
        [ $? -ne 0 ] && {
            echo "$STR_FAILED"
            return
        }
        echo "$STR_OK"
        ;;
    "nodered")
        _stop_pidname "sscma-node" 1
        _stop_pidname "node"
        /etc/init.d/S03node-red restart >/dev/null 2>&1
        [ $? -ne 0 ] && {
            echo "$STR_FAILED"
            return
        }
        echo "$STR_OK"
        ;;
    *)
        echo "$STR_FAILED"
        return
        ;;
    esac
}
# deamon
##################################################

# call function
$1 "$@"
