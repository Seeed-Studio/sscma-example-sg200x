#!/bin/bash
readonly DIR_SCRIPTS=$(dirname $(readlink -f $0))
readonly STR_OK="OK"
readonly STR_FAILED="Failed"

# conf
readonly USER_NAME="recamera"
readonly CONFIG_DIR="/etc/$USER_NAME.conf"
readonly CONF_UPGRADE="$CONFIG_DIR/upgrade"

# userdata
readonly USERDATA_DIR="/userdata"
readonly MODEL_DIR="$USERDATA_DIR/Models"
readonly MODELS_PRESET="/usr/share/supervisor/models"

# work_dir
readonly WORK_DIR="/tmp/supervisor"
if [ ! -d "$WORK_DIR" ]; then
    mkdir -p "$WORK_DIR"
    chmod 400 -R "$WORK_DIR"
fi

# private
_compatible() {
    [ ! -d "$CONFIG_DIR" ] && mkdir -p "$CONFIG_DIR"
    [ -f "/etc/upgrade" ] && mv /etc/upgrade "$CONF_UPGRADE"
}
_compatible

_ip() { ifconfig "$1" 2>/dev/null | awk '/inet addr:/ {print $2}' | awk -F':' '{print $2}'; }
_mask() { ifconfig "$1" 2>/dev/null | awk '/Mask:/ {print $4}' | awk -F':' '{print $2}'; }
_mac() { ifconfig "$1" 2>/dev/null | awk '/HWaddr/ {print $5}'; }
_gateway() { route -n | grep '^0.0.0.0' | grep -w "$1" | awk '{print $2}'; }
_dns() { cat /etc/resolv.conf 2>/dev/null | awk '/nameserver/ {print $2}' | head -n 1; }

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
readonly ISSUE_FILE="/etc/issue"
readonly HOSTNAME_FILE="/etc/hostname"
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
    printf '{'
    printf '"channel": "%s",' "$((ch))"
    printf '"serverUrl": "%s",' "$url"
    printf '"needRestart": %s,' "$(_upgrade_done)"
    printf '"ethIp": "%s",' "$(_ip "eth0")"
    printf '"wifiIp": "%s",' "$(_ip "wlan0")"
    printf '"mask": "%s",' "$(_mask "wlan0")"
    printf '"mac": "%s",' "$(_mac "wlan0")"
    printf '"gateway": "%s",' "$(_gateway "wlan0")"
    printf '"dns": "%s",' "$(_dns)"
    printf '"deviceName": "%s" }' "$(cat $HOSTNAME_FILE)"
}

# flow
_check_flow() {
    local src_flow="/usr/share/supervisor/flows.json"
    local src_flow_gimbal="/usr/share/supervisor/flows_gimbal.json"
    local dst_flow="/home/recamera/.node-red/flows.json"

    [ ! -f "$dst_flow" ] && {
        [ -n "$(ifconfig can0 2>/dev/null | grep "HWaddr")" ] && {
            [ -f "$src_flow_gimbal" ] && src_flow=$src_flow_gimbal
        }
        [ -f "$src_flow" ] && {
            cp -f "$src_flow" "$dst_flow"
            chown "$USER_NAME":"$USER_NAME" "$dst_flow"
            sync
        }
    }
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
readonly UPGRADE="$DIR_SCRIPTS/upgrade.sh"
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
}

function cancelUpdate() {
    echo 1 >"$UPGRADE_CANCEL"
    rm -rf $UPGRADE_PROG
    $UPGRADE download x >/dev/null 2>&1 &
    $UPGRADE start x >/dev/null 2>&1 &
    echo "$STR_OK"
}

function getSystemUpdateVersion() {
    local out="" url=""
    local ch=$(cat $CONF_UPGRADE 2>/dev/null | awk -F'[, ]' '{print $1}')
    if [ $((ch)) -ne 0 ]; then
        url=$(cat $CONF_UPGRADE 2>/dev/null | awk -F'[, ]' '{print $2}')
    fi
    local os="" ver=""
    for i in {1..3}; do
        $UPGRADE latest $url >/dev/null 2>&1
        out=$($UPGRADE latest q)
        [ "$(echo $out | cut -d':' -f1)" = "Success" ] && {
            out2=$(echo $out | cut -d':' -f2)
            os=$(echo $out2 | cut -d' ' -f1)
            ver=$(echo $out2 | cut -d' ' -f2)
            break
        }
    done

    # result
    if [ -z "$os" ] || [ -z "$ver" ]; then
        echo "$out"
        return 1
    fi
    printf '{"osName": "%s", "osVersion": "%s", "downloadUrl": "%s", "isUpgrading": "%s"}' \
        "$os" "$ver" ${url:-$DEFAULT_UPGRADE_URL} "$(_upgrading)"
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
    local ttyd_port=9090
    _stop_pidname "ttyd"
    ttyd -p $ttyd_port -u $USER_NAME login >/dev/null 2>&1 &

    local rollback=0
    [ "$(fw_printenv boot_rollback)" = "boot_rollback=1" ] && rollback=1

    _check_flow >/dev/null 2>&1
    _check_models >/dev/null 2>&1

    printf '{'
    printf '"sn": "%s",' "$(_sn)"
    printf '"dev_name": "%s",' "$(_dev_name)"
    printf '"os": "%s",' "$(_os_name)"
    printf '"ver": "%s",' "$(_os_version)"
    printf '"cpu": "sg2002",'
    printf '"ram": "256",'
    printf '"npu": "1",'
    printf '"ws": "8000",'
    printf '"ttyd": "%d",' "$ttyd_port"
    printf '"rollback": "%d",' "$rollback"
    printf '"app_dir": "%s",' "$APP_DIR"
    printf '"url": "%s",' "$DEFAULT_UPGRADE_URL"
    printf '"platform_info": "%s",' "$CONFIG_DIR/platform.info"
    printf '"model": { "dir": "%s", "file": "%s", "info": "%s", "preset": "%s" }' \
        "$MODEL_DIR" "$MODEL_FILE" "$MODEL_INFO" "$MODELS_PRESET"
    printf '}'
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
readonly SSH_SERVICE="S*sshd"

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

function setSShStatus() {
    local dir="$DIR_INID"
    local dir_disabled="$DIR_INID_DISABLED"
    local ret=0

    if [ $((2)) -eq 0 ]; then
        path=$(ls $dir/$SSH_SERVICE 2>/dev/null)
        if [ -n "$path" ] && [ -f $path ]; then
            mkdir -p $dir_disabled
            $path stop >/dev/null 2>&1
            mv $path $dir_disabled/
            ret=$?
        fi
    else
        path=$(ls $dir_disabled/$SSH_SERVICE 2>/dev/null)
        if [ -n "$path" ] && [ -f $path ]; then
            $path restart >/dev/null 2>&1
            mv $path $dir/
            ret=$?
        fi
    fi

    [ 0 -ne $((ret)) ] && echo $STR_FAILED || echo $STR_OK
}

function queryUserInfo() {
    local firstLogin="false"
    local sshEnabled="false"
    local sshKeyFile="$WORK_DIR/sshkey"

    [ -f "$FIRST_LOGIN" ] && firstLogin="true"
    [ -n "$(ls -l $DIR_INID/$SSH_SERVICE 2>/dev/null)" ] && sshEnabled="true"
    [ -f "$sshKeyFile" ] || >"$sshKeyFile"

    _update_sshkey
    printf '{"userName": "%s", "firstLogin": %s, "sshEnabled": %s, "sshKeyFile": "%s" }' \
        "${USER_NAME}" "${firstLogin}" "${sshEnabled}" "${sshKeyFile}"
}

function updatePassword() {
    local pwd="$2"
    passwd $USER_NAME 1>/dev/null 2>/dev/null <<EOF
"$pwd"
"$pwd"
EOF
    [ 0 -ne $? ] && echo $STR_FAILED || echo $STR_OK
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
_ap_stop() { _stop_pidname "hostapd"; }

_sta_start() {
    _check_wifi || return 0
    [ -z "$(pidof wpa_supplicant)" ] && {
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
    [ -z "$(pidof hostapd)" ] && {
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
    [ -z "$1" ] && { $WPA_CLI reconfigure; }
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
    printf '{"ip": "%s", "mask": "%s", "mac": "%s", "gateway": "%s", "dns1": "", "dns2": ""}' \
        "$(_ip "$ifname")" "$(_mask "$ifname")" \
        "$(_mac "$ifname")" "$(_gateway "$ifname")"
}

function getWiFiScanResults() {
    local out=$WORK_DIR/${FUNCNAME[0]}
    local result="$out.tmp"
    [ -s "$result" ] && {
        cp "$result" "$out"
        rm "$result"
    }
    [ -f "$out" ] || >"$out"
    echo "$out"
    [ -z "$(pidof iw)" ] && {
        iw dev wlan0 scan >"$result" 2>/dev/null &
    }
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
    _sta_stop >/dev/null 2>&1
    _sta_start >/dev/null 2>&1 &
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
# deamon
function query_sscma() {
    mosquitto_rr -h localhost -p 1883 -q 1 -v -W 3 \
        -t "sscma/v0/recamera/node/in/" \
        -e "sscma/v0/recamera/node/out/" \
        -m "{\"name\":\"health\",\"type\":3,\"data\":\"\"}" 2>/dev/null
    [ $? -ne 0 ] && {
        echo "$STR_FAILED"
        return
    }
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
