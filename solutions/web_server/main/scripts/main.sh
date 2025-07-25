#!/bin/bash
DIR_SCRIPTS=$(dirname $(readlink -f $0))
STR_OK="OK"
STR_FAILED="Failed"

# conf
CONFIG_DIR="/etc/recamera.conf"

# userdata
USER_DIR="/userdata"
APP_DIR="$USER_DIR/app"

# work_dir
WORK_DIR=/tmp/web_server
if [ ! -d $WORK_DIR ]; then
    mkdir -p $WORK_DIR
    chmod 400 -R $WORK_DIR
fi

# private
_compatible() {
    [ ! -d "$CONFIG_DIR" ] && mkdir -p "$CONFIG_DIR"
    [ -f "/etc/upgrade" ] && mv /etc/upgrade $CONF_UPGRADE
}
_compatible

_stop_pidname() { for pid in $(pidof "$1"); do [ -d "/proc/$pid" ] && kill -9 $pid; done; }

_ip() { ifconfig "$1" 2>/dev/null | awk '/inet addr:/ {print $2}' | awk -F':' '{print $2}'; }
_mask() { ifconfig "$1" 2>/dev/null | awk '/Mask:/ {print $4}' | awk -F':' '{print $2}'; }
_mac() { ifconfig "$1" 2>/dev/null | awk '/HWaddr/ {print $5}'; }
_gateway() { route -n | grep '^0.0.0.0' | grep -w "$1" | awk '{print $2}'; }

_sn() { fw_printenv sn | awk -F'=' '{print $NF}'; }
_dev_name() { cat "$HOSTNAME_FILE"; }
_os_name() { cat "$ISSUE_FILE" 2>/dev/null | awk -F' ' '{print $1}'; }
_os_version() { cat "$ISSUE_FILE" 2>/dev/null | awk -F' ' '{print $2}'; }
_wifi_status() { wpa_cli -i "$1" status 2>/dev/null | grep "^wpa_state" | awk -F= '{print $2}'; }

##################################################
# file
api_file() {
    [ -d $APP_DIR ] || mkdir -p $APP_DIR
    echo $APP_DIR
}
# file
##################################################

##################################################
# device
WEB_SOCKET_PORT=8000
TTYD_PORT=9090
CPU_NAME=sg2002
RAM_SIZE=256
NPM_NUM=1

ISSUE_FILE="/etc/issue"
HOSTNAME_FILE="/etc/hostname"
AVAHI_CONF="/etc/avahi/avahi-daemon.conf"
AVAHI_SERVICE="/etc/init.d/S50avahi-daemon"

# platform
function getPlatformInfo() {
    local path="$CONFIG_DIR/platform.info"
    [ -f "$path" ] || >"$path"
    echo "$path"
}

function savePlatformInfo() {
    getPlatformInfo
}

# device
function getCameraWebsocketUrl() {
    echo "$WEB_SOCKET_PORT"
}

function getDeviceInfo() {
    printf '{"sn": "%s", "deviceName": "%s", "status": 1, "osName": "%s", "osVersion": "%s" }' \
        "$(_sn)" "$(_dev_name)" "$(_os_name)" "$(_os_version)"
}

function getDeviceList() {
    local out="$WORK_DIR/${FUNCNAME[0]}"
    local tmp="$out.tmp"
    [ -s "$tmp" ] && { cp "$tmp" "$out"; }
    [ -z "$(pidof avahi-browse 2>/dev/null)" ] && {
        avahi-browse -arpt >"$tmp" 2>/dev/null &
    }
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
    local dev_name=$(cat $HOSTNAME_FILE)
    local sn=$(fw_printenv sn | awk -F'=' '{print $NF}')
    local eth wifi mask mac gateway dns

    eth=$(_ip "eth0")
    local ifname="wlan0"
    if [ $(_wifi_status $ifname) = "COMPLETED" ]; then
        wifi=$(_ip $ifname)
    else
        ifname="eth0"
    fi
    mask=$(_mask $ifname)
    mac=$(_mac $ifname)
    gateway=$(_gateway $ifname)

    [ -z "$eth" ] && eth="-"
    [ -z "$wifi" ] && wifi="-"
    [ -z "$mask" ] && mask="-"
    [ -z "$mac" ] && mac="-"
    [ -z "$gateway" ] && gateway="-"
    dns="$gateway"

    local ch=$(cat $CONF_UPGRADE 2>/dev/null | awk -F',' '{print $1}')
    local url=$(cat $CONF_UPGRADE 2>/dev/null | awk -F',' '{print $2}')
    [ -z $ch ] && ch=0

    printf '{"deviceName": "%s", "sn": "%s",
"ethIp": "%s",
"wifiIp": "%s", "mask": "%s", "mac": "%s", "gateway": "%s", "dns": "%s",
"channel": %s, "serverUrl": "%s", "officialUrl": "%s",
"osName": "%s", "osVersion": "%s",
"cpu": "%s", "ram": %s, "npu": %s,
"terminalPort": %s, "needRestart": %s
}' \
        "${dev_name}" "${sn}" \
        "${eth}" \
        "${wifi}" "${mask}" "${mac}" "${gateway}" "${dns}" \
        "${ch}" "${url}" "${DEFAULT_UPGRADE_URL}" \
        "$(_os_name)" "$(_os_version)" \
        "${CPU_NAME}" "${RAM_SIZE}" "${NPM_NUM}" \
        "${TTYD_PORT}" "$(_upgrade_done)"
}

# system
function getSystemStatus() {
    local s="$(fw_printenv boot_rollback)"
    local ret="$STR_FAILED"

    if [ "$s" = "boot_rollback=" ]; then
        ret="$STR_OK"
    elif [ "$s" = "boot_rollback=1" ]; then
        ret="System has been recovered from damage."
    fi
    echo "$ret"
}

function queryServiceStatus() {
    local sscma=0
    local nodered=0
    local system=0
    printf '{"sscmaNode": %d, "nodeRed": %d, "system": %d }' \
        "${sscma}" "${nodered}" "${system}"
}

function setPower() {
    local mode=$2
    if [ "$mode" = "0" ]; then
        echo "Poweroff"
        poweroff
    elif [ "$mode" = "1" ]; then
        echo "Reboot"
        reboot
    else
        echo $STR_FAILED
    fi
}

# model
MODELS_DIR="/usr/share/supervisor/models"
MODEL_DIR="$USER_DIR/MODEL"
MODEL_SUFFIX=".cvimodel"
MODEL_FILE="$MODEL_DIR/model${MODEL_SUFFIX}"
MODEL_INFO="$MODEL_DIR/model.json"

function getModelFile() {
    local path=""
    local md5=""
    if [ -f "$MODEL_FILE" ]; then
        path=$MODEL_FILE
        md5=$(md5sum $path | awk '{print $1}')
    fi
    printf '{"file": "%s", "md5": "%s" }' \
        "${path}" "${md5}"
}

function getModelInfo() {
    local path="" info="" md5=""
    if [ -f "$MODEL_FILE" ] && [ -f "$MODEL_INFO" ]; then
        path=$MODEL_FILE
        info=$MODEL_INFO
        md5=$(md5sum $path | awk '{print $1}')
    fi
    printf '{"file": "%s", "model_md5": "%s" }' \
        "${info}" "${md5}"
}

function getModelList() {
    local json=$(ls $MODELS_DIR/*.json 2>/dev/null)
    printf '{"list": ['
    for i in $json; do
        local name=${i%.*}
        [ -f "${name}${MODEL_SUFFIX}" ] && printf '"%s",' $name
    done
    printf '""], "suffix": "%s" }' ${MODEL_SUFFIX}
}

function uploadModel() { echo "$MODEL_DIR"; }

# upgrade
DEFAULT_UPGRADE_URL="https://github.com/Seeed-Studio/reCamera-OS/releases/latest"
CONF_UPGRADE="$CONFIG_DIR/upgrade"
UPGRADE="$DIR_SCRIPTS/upgrade.sh"

UPGRADE_CANCEL="$WORK_DIR/upgrade.cancel"
UPGRADE_DONE="$WORK_DIR/upgrade.done"
UPGRADE_PROG="$WORK_DIR/upgrade.prog"

_upgrade_done() { [ -f "$UPGRADE_DONE" ] && echo "1" || echo "0"; }

function updateChannel() {
    if [ -f "$UPGRADE_PROG" ]; then
        echo "It's upgrading now..."
        return 1
    fi

    local ch=$2
    local url=$3
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
    local out
    for i in {1..3}; do
        out=$($UPGRADE latest q)
        [ "$(echo $out | cut -d':' -f1)" = "Success" ] && {
            local out2 os_name os_version url upgrade
            out2=$(echo $out | cut -d':' -f2)
            os_name=$(echo $out2 | cut -d' ' -f1)
            os_version=$(echo $out2 | cut -d' ' -f2)
            url=$(cat /tmp/upgrade/url.txt 2>/dev/null)
            out2=$($UPGRADE download q)
            [ "$out2" != "Stopped" ] && upgrade=1
            printf '{"osName": "%s", "osVersion": "%s", "downloadUrl": "%s", "isUpgrading": "%d"}' \
                $os_name $os_version $url $((upgrade))
            return 0
        }
        $UPGRADE latest >/dev/null 2>&1
    done
    echo "$out"
}

_upgrade_prog() {
    local mutex=$WORK_DIR/${FUNCNAME[0]}
    [ -f "$mutex" ] && { return 0; }
    >$mutex
    local result size total prog1 prog2
    # download
    read -r result size total <<<"$($UPGRADE download q | awk -F'[: ]' '{print $1, $3, $4}')"
    if [[ "$result" == "Failed" || "$result" == "Stopped" ]]; then
        $UPGRADE download >/dev/null 2>&1 &
        printf '{"progress": "0", "status": "download"}'
        return 0
    elif [ "$result" != "Success" ]; then
        [ $((total)) -gt 0 ] && prog1=$((size * 100 / total))
        printf '{"progress": "%d", "status": "download"}' "$((prog1 / 2))"
        return 0
    fi
    prog1=100

    # upgrade
    read -r result size total <<<"$($UPGRADE start q | awk -F'[: ]' '{print $1, $3, $4}')"
    if [[ "$result" == "Failed" || "$result" == "Stopped" ]]; then
        $UPGRADE start >/dev/null 2>&1 &
        printf '{"progress": "50", "status": "upgrade"}'
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
        echo '{"progress": "0", "status": "download"}' >$UPGRADE_PROG
    fi
    cat $UPGRADE_PROG

    _upgrade_prog >$UPGRADE_PROG.tmp 2>/dev/null &
}

function updateSystem() {
    rm -f "$WORK_DIR/upgrade"*
    echo $STR_OK
}
# device
##################################################

##################################################
# user
USER_NAME=recamera
SSH_DIR="/home/$USER_NAME/.ssh"
SSH_KEY_FILE="$SSH_DIR/authorized_keys"

FIRST_LOGIN="/etc/.first_login"
DIR_INID="/etc/init.d"
DIR_INID_DISABLED="$DIR_INID/disabled"
SSH_SERVICE="S*sshd"

# private
function _is_key_valid() {
    local tmp=$(mktemp)
    local ret=0

    echo "$*" >$tmp
    ssh-keygen -lf $tmp 2>/dev/null
    ret=$?
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
function gen_token() {
    dd if=/dev/random bs=64 count=1 2>/dev/null | base64 -w0
}

function get_username() {
    echo $USER_NAME
}

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

function login() {
    [ -f "$FIRST_LOGIN" ] && rm -f "$FIRST_LOGIN"
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
CONF_WIFI="$CONFIG_DIR/wifi"

# alias
WPA_CLI="wpa_cli -i wlan0"
STA_DOWN="ifconfig wlan0 down >/dev/null 2>&1"
STA_UP="ifconfig wlan0 up >/dev/null 2>&1"
AP_DOWN="ifconfig wlan1 down >/dev/null 2>&1"
AP_UP="ifconfig wlan1 up >/dev/null 2>&1"

_check_wifi() { [ -z "$(ifconfig wlan0 2>/dev/null)" ] && return 1 || return 0; }
_sta_stop() { _stop_pidname "wpa_supplicant"; }
_ap_stop() { _stop_pidname "hostapd"; }

_sta_start() {
    _check_wifi || return 0
    $STA_DOWN
    $STA_UP
    wpa_supplicant -B -i wlan0 -c "/etc/wpa_supplicant.conf" >/dev/null 2>&1
}

_ap_start() {
    _check_wifi || return 0
    $AP_DOWN
    $AP_UP
    hostapd -B "/etc/hostapd_2g4.conf" >/dev/null 2>&1
}

start_wifi() {
    _check_wifi || {
        echo "-1"
        return 0
    }

    [ ! -f "$CONF_WIFI" ] && echo 1 >"$CONF_WIFI"
    local on=$(cat "$CONF_WIFI")
    [ "$on" = "1" ] && {
        _sta_start >/dev/null 2>&1 &
        _ap_start >/dev/null 2>&1 &
    }
    echo "$on"
}

stop_wifi() {
    _sta_stop >/dev/null 2>&1 &
    _ap_stop >/dev/null 2>&1 &
}

switchWiFi() {
    echo $2 >"$CONF_WIFI"
    [ "$2" = "0" ] && stop_wifi || start_wifi
}

get_sta_connected() {
    local out=$WORK_DIR/${FUNCNAME[0]}
    >"$out"
    $WPA_CLI list_networks 2>/dev/null | while IFS= read -r line; do
        printf "%b\n" "$line" >>"$out"
    done
    echo "$out"
}

get_sta_current() {
    local out=$WORK_DIR/${FUNCNAME[0]}
    >"$out"
    $WPA_CLI status 2>/dev/null | while IFS= read -r line; do
        printf "%b\n" "$line" >>"$out"
    done
    echo "$out"
}

get_eth() {
    local ifname="eth0"
    printf '{"ip": "%s", "mask": "%s", "mac": "%s", "gateway": "%s", "dns1": "", "dns2": ""}' \
        "$(_ip "$ifname")" "$(_mask "$ifname")" "$(_mac "$ifname")" "$(_gateway "$ifname")"
}

_get_scan_results() {
    for ((i = 1; i <= 5; i++)); do
        [ "$($WPA_CLI scan 2>/dev/null)" == "OK" ] && break
        sleep 0.5
    done
    sleep 3

    local out=$WORK_DIR/${FUNCNAME[0]}
    >"$out"
    $WPA_CLI scan_results 2>/dev/null | while IFS= read -r line; do
        printf "%b\n" "$line" >>"$out"
    done
}

getWiFiScanResults() {
    local out=$WORK_DIR/${FUNCNAME[0]}
    local result="$WORK_DIR/_get_scan_results"

    [ -s "$result" ] && { cp "$result" "$out"; }
    _get_scan_results >/dev/null 2>&1 &

    [ -f "$out" ] || >"$out"
    echo "$out"
}

connectWiFi() {
    local id="$2" ssid="\"$3\"" pwd="\"$4\""

    $WPA_CLI reconfigure >/dev/null 2>&1
    [ $((id)) -lt 0 ] && { # create new
        id=$($WPA_CLI add_network)

        local ret=$($WPA_CLI set_network "$id" ssid "$ssid")
        [ "$ret" != "OK" ] && {
            $WPA_CLI remove_network "$id" >/dev/null 2>&1
            return
        }

        [ -z "$pwd" ] && {
            $WPA_CLI set_network "$id" key_mgmt NONE >/dev/null 2>&1
        } || {
            ret=$($WPA_CLI set_network "$id" psk "$pwd")
            [ "$ret" != "OK" ] && {
                $WPA_CLI remove_network "$id" >/dev/null 2>&1
                return
            }
        }
    }

    local ids=$(cat $(get_sta_connected) | awk -F'\t' '{print $1}')
    for i in $ids; do
        $WPA_CLI set_network "$i" priority 0 >/dev/null 2>&1
    done

    [ $((id)) -ge 0 ] && {
        $WPA_CLI set_network "$id" priority 100 >/dev/null 2>&1
        $WPA_CLI enable_network "$id" >/dev/null 2>&1
    }

    $WPA_CLI save_config
    $WPA_CLI select_network "$id" >/dev/null 2>&1
}

_wpa_set_networks() {
    local ssid="$1"
    local status="$2"
    $WPA_CLI reconfigure >/dev/null 2>&1
    local id=$(cat $(get_sta_connected) | grep -w "$ssid" | awk -F'\t' '{print $1}')
    [ -z "$id" ] && return 0
    $WPA_CLI "$status" "$id" >/dev/null 2>&1
    $WPA_CLI save_config
}

disconnectWiFi() {
    _wpa_set_networks "$2" disable_network
}

forgetWiFi() {
    _wpa_set_networks "$2" remove_network
}
# network
##################################################

# call function
$1 "$@"
