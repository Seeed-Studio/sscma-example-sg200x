#!/bin/bash

DIR_SCRIPTS=$(dirname $(readlink -f $0))
UPGRADE="$DIR_SCRIPTS/upgrade.sh"

STR_OK="OK"
STR_FAILED="Failed"
WORK_DIR=/tmp/web_server

CPU_NAME=sg2002
RAM_SIZE=256
NPM_NUM=1
USER_NAME=recamera
SSH_DIR="/home/$USER_NAME/.ssh"
SSH_KEY_FILE="$SSH_DIR/authorized_keys"

HOSTNAME_FILE="/etc/hostname"
ISSUE_FILE="/etc/issue"
FIRST_LOGIN="/etc/.first_login"
INID_DIR="/etc/init.d"
INID_DIR_DISABLED="$INID_DIR/disabled"
SSH_SERVICE="S*sshd"
WEB_SOCKET_PORT=8000
TTYD_PORT=9090

AVAHI_CONF="/etc/avahi/avahi-daemon.conf"
AVAHI_SERVICE="/etc/init.d/S50avahi-daemon"
MODELS_DIR="/usr/share/supervisor/models"
PLATFORM_INFO="/usr/share/supervisor/platform.info"

CONFIG_DIR="/etc/recamera.conf"

CONF_UPGRADE="$CONFIG_DIR/upgrade"
DEFAULT_UPGRADE_URL="https://github.com/Seeed-Studio/reCamera-OS/releases/latest"

USER_DIR="/userdata"
APP_DIR="$USER_DIR/app"
MODEL_DIR="$USER_DIR/MODEL"
MODEL_SUFFIX=".cvimodel"
MODEL_FILE="$MODEL_DIR/model${MODEL_SUFFIX}"
MODEL_INFO="$MODEL_DIR/model.json"

CONF_WIFI="$CONFIG_DIR/wifi"
WPA_SUPPLICANT_CONF="/etc/wpa_supplicant.conf"
HOSTAPD_CONF="/etc/hostapd_2g4.conf"
WPA_CLI="wpa_cli -i wlan0"

function compatible() {
    [ ! -d "$CONFIG_DIR" ] && mkdir -p "$CONFIG_DIR"
    [ -f "/etc/upgrade" ] && mv /etc/upgrade $CONF_UPGRADE
}
compatible

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

function gen_token() {
    dd if=/dev/random bs=64 count=1 2>/dev/null | base64 -w0
}

function get_username() {
    echo $USER_NAME
}

function _update_sshkey() {
    local sshKeyFile=$WORK_DIR/sshkey
    >$sshKeyFile
    if [ -f $SSH_KEY_FILE ]; then
        local cnt=1
        while IFS= read -r line; do
            if [[ "$line" =~ ^#.*$ ]]; then # skip comment line
                continue
            fi

            local sh256=$(_is_key_valid $line)
            if [ -n "$sh256" ]; then
                echo "$cnt $sh256" >>$sshKeyFile
                let cnt++
            fi
        done <$SSH_KEY_FILE
    fi
}

# api_led
function led() {
    local led=$2
    local value=$3
    local path="/sys/class/leds/${led}/brightness"

    if [ ! -f $path ]; then
        echo "Invalid led=$led"
        exit 1
    fi
    if [ "$value" == "on" ]; then
        echo 1 >$path
    elif [ "$value" == "off" ]; then
        echo 0 >$path
    else
        echo "Invalid value=$value"
        exit 1
    fi
    echo $STR_OK
}

# api_user
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
        echo "Exist"
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

    total_lines=$(wc -l <"$SSH_KEY_FILE" 2>/dev/null || echo 0)
    if [ "$line" -lt 1 ] || [ "$line" -gt "$total_lines" ]; then
        echo "Invalid id=$line"
        exit 1
    fi

    # Todo: skip comment line
    sed -i "${line}d" "$SSH_KEY_FILE"
    echo $STR_OK
}

function login() {
    [ -f $FIRST_LOGIN ] && rm -f $FIRST_LOGIN
}

function queryUserInfo() {
    local firstLogin="false"
    local sshEnabled="false"
    local sshKeyFile=$WORK_DIR/sshkey

    [ -f $FIRST_LOGIN ] && firstLogin="true"
    [ -n "$(ls -d $INID_DIR/$SSH_SERVICE 2>/dev/null)" ] && sshEnabled="true"
    [ -f $sshKeyFile ] || sshKeyFile=""

    _update_sshkey
    printf '{"userName": "%s", "firstLogin": %s, "sshEnabled": %s, "sshKeyFile": "%s" }' \
        "${USER_NAME}" "${firstLogin}" "${sshEnabled}" "${sshKeyFile}"
}

function setSShStatus() {
    local dir="$INID_DIR"
    local dir_disabled="$INID_DIR_DISABLED"
    local sshd="$SSH_SERVICE"
    local ret=0

    if [ $2 -eq 0 ]; then
        path=$(ls $dir/$sshd 2>/dev/null)
        if [ -n "$path" ] && [ -f $path ]; then
            mkdir -p $dir_disabled
            $path stop >/dev/null 2>&1
            mv $path $dir_disabled/
            ret=$?
        fi
    else
        path=$(ls $dir_disabled/$sshd 2>/dev/null)
        if [ -n "$path" ] && [ -f $path ]; then
            $path restart >/dev/null 2>&1
            mv $path $dir/
            ret=$?
        fi
    fi

    if [ 0 -ne $ret ]; then
        echo $STR_FAILED
    else
        echo $STR_OK
    fi
}

function updatePassword() {
    passwd $USER_NAME 1>/dev/null 2>/dev/null <<EOF
$3
$3
EOF
    if [ 0 -ne $? ]; then
        echo $STR_FAILED
    else
        echo $STR_OK
    fi
}

# api_device
function _os_name() {
    cat $ISSUE_FILE 2>/dev/null | awk -F' ' '{print $1}'
}

function _os_version() {
    cat $ISSUE_FILE 2>/dev/null | awk -F' ' '{print $2}'
}

function _wifi_status() {
    local ifname=$1
    wpa_cli -i $ifname status 2>/dev/null | grep "^wpa_state" | awk -F= '{print $2}'
}

function _ip() {
    local ifname=$1
    ifconfig $ifname 2>/dev/null | awk '/inet addr:/ {print $2}' | awk -F':' '{print $2}'
}

function _mask() {
    local ifname=$1
    ifconfig $ifname 2>/dev/null | awk '/Mask:/ {print $4}' | awk -F':' '{print $2}'
}

function _mac() {
    local ifname=$1
    ifconfig $ifname 2>/dev/null | awk '/HWaddr/ {print $5}'
}

function _gateway() {
    local ifname=$1
    route -n | grep '^0.0.0.0' | grep -w $ifname | awk '{print $2}'
}

function getCameraWebsocketUrl() {
    echo $WEB_SOCKET_PORT
}

function getDeviceInfo() {
    local sn=$(fw_printenv sn | awk -F'=' '{print $NF}')
    local dev_name=$(cat $HOSTNAME_FILE)
    local status=1
    local os_name=$(_os_name)
    local os_version=$(_os_version)

    printf '{"sn": "%s", "deviceName": "%s", "status": %d, "osName": "%s", "osVersion": "%s" }' \
        "${sn}" "${dev_name}" "${status}" "${os_name}" "${os_version}"
}

function updateDeviceName() {
    local dev_name=$2
    if [ -z $dev_name ]; then
        echo $STR_FAILED
        exit 1
    fi

    echo $dev_name >$HOSTNAME_FILE
    hostname -F $HOSTNAME_FILE
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

    local os_name=$(_os_name)
    local os_version=$(_os_version)
    local upgraded=0

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
        "${os_name}" "${os_version}" \
        "${CPU_NAME}" "${RAM_SIZE}" "${NPM_NUM}" \
        "${TTYD_PORT}" "${upgraded}"
}

function updateChannel() {
    # Todo: check upgrade
    # echo "It's upgrading now..."
    local ch=$2
    local url=$3
    echo $ch,$url >$CONF_UPGRADE
    echo $STR_OK
}

function getModelFile() {
    local path=$MODEL_FILE
    local md5=$(md5sum $path | awk '{print $1}')
    printf '{"file": "%s", "md5": "%s" }' \
        "${path}" "${md5}"
}

function getModelInfo() {
    local path=$MODEL_FILE
    md5=$(md5sum $path | awk '{print $1}')
    printf '{"file": "%s", "model_md5": "%s" }' \
        "${MODEL_INFO}" "${md5}"
}

function getModelList() {
    local json=$(ls $MODELS_DIR/*.json 2>/dev/null)

    printf '{"list": ['
    local cnt=0
    for i in $json; do
        local name=${i%.*}
        if [ -f "${name}${MODEL_SUFFIX}" ]; then
            printf '"%s",' $name
            let cnt++
        fi
    done
    printf '""], "count": %d, "suffix": "%s" }' $cnt ${MODEL_SUFFIX}
}

function uploadModel() {
    echo $MODEL_DIR
}

function savePlatformInfo() {
    echo $PLATFORM_INFO
}

function getPlatformInfo() {
    if [ -f $PLATFORM_INFO ]; then
        echo $PLATFORM_INFO
    else
        echo ""
    fi
}

function getSystemStatus() {
    local s="$(fw_printenv boot_rollback)"

    if [ "$s" = "boot_rollback=" ]; then
        echo $STR_OK
    elif [ "$s" = "boot_rollback=1" ]; then
        echo "System has been recovered from damage."
    else
        echo $STR_FAILED
    fi
}

function setPower() {
    local mode=$2
    if [ $mode -eq 0 ]; then
        echo "Poweroff"
        poweroff
    elif [ $mode -eq 1 ]; then
        echo "Reboot"
        reboot
    else
        echo $STR_FAILED
    fi
}

function queryServiceStatus() {
    local sscma=0
    local nodered=0
    local system=0
    printf '{"sscmaNode": %d, "nodeRed": %d, "system": %d }' \
        "${sscma}" "${nodered}" "${system}"
}

function getDeviceList() {
    local out=$WORK_DIR/${FUNCNAME[0]}
    local tmp=$WORK_DIR/${file}.tmp
    if [ -f $tmp ]; then
        cp $tmp $out
        rm $tmp
    fi
    avahi-browse -arpt >$tmp &

    [ ! -f $out ] && >$out
    printf '{"file": "%s"}' $out
}

# api_file
function deleteFile() {
    local fname=$2
    if [ -z $fname ]; then
        echo $STR_FAILED
        exit 1
    fi

    local path=$APP_DIR/$2
    if [ -f $path ]; then
        rm -f $path
        echo $STR_OK
    else
        echo $STR_FAILED
    fi
}

function queryFileList() {
    local list=$(ls $APP_DIR/ 2>/dev/null)

    printf '{"fileList": ['
    local cnt=0
    for i in $list; do
        printf '"%s",' $i
        let cnt++
    done
    printf '""], "count": %d }' $cnt
}

function uploadFile() {
    if [ ! -d $APP_DIR ]; then
        mkdir -p $APP_DIR
    fi
    echo "$APP_DIR"
}

function cancelUpdate() {
    echo 1 >$WORK_DIR/upgrade.cancel
    $UPGRADE download x >/dev/null 2>&1 &
    $UPGRADE start x >/dev/null 2>&1 &
    echo "$STR_OK"
}

function getSystemUpdateVesionInfo() { # typo
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

function getUpdateProgress() {
    [ -f "$WORK_DIR/upgrade.cancel" ] && {
        echo "Cancelled"
        return 0
    }
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
    printf '{"progress": "%d", "status": "upgrade"}' $(((prog1 + prog2) / 2))
}

function updateSystem() {
    rm -f "$WORK_DIR/upgrade"*
    echo $STR_OK
}

# wifi
function _check_wifi() {
    if [ -z "$(ifconfig wlan0 2>/dev/null)" ]; then
        return 1 # no wifi
    fi
    return 0
}

function _sta_start() {
    _check_wifi || return 0
    ifconfig wlan0 down
    ifconfig wlan0 up
    wpa_supplicant -B -i wlan0 -c $WPA_SUPPLICANT_CONF >/dev/null 2>&1
}

function _sta_stop() {
    _check_wifi || return 0
    local pids=$(pidof "wpa_supplicant")
    for pid in $pids; do
        [ -d "/proc/$pid" ] && kill -9 $pid
    done
}

function _ap_start() {
    _check_wifi || return 0
    ifconfig wlan1 down >/dev/null 2>&1
    ifconfig wlan1 up >/dev/null 2>&1
    hostapd -B $HOSTAPD_CONF >/dev/null 2>&1
}

function _ap_stop() {
    _check_wifi || return 0
    local pids=$(pidof "hostapd")
    for pid in $pids; do
        [ -d "/proc/$pid" ] && kill -9 $pid
    done
    ifconfig wlan1 down >/dev/null 2>&1
}

function start_wifi() {
    _check_wifi || {
        echo "-1"
        return 0
    }

    [ ! -f "$CONF_WIFI" ] && echo 1 >"$CONF_WIFI"
    local on=$(cat "$CONF_WIFI")
    if [ "$on" = "1" ]; then
        _sta_start >/dev/null 2>&1 &
        _ap_start >/dev/null 2>&1 &
    fi
    echo "$on"
}

function stop_wifi() {
    _sta_stop
    _ap_stop
}

function switchWiFi() {
    echo $2 >"$CONF_WIFI"
    if [ "$2" = "0" ]; then
        stop_wifi
    else
        start_wifi
    fi
}

function get_networks() {
    local out=$WORK_DIR/${FUNCNAME[0]}
    >"$out"
    $WPA_CLI list_networks 2>/dev/null | while IFS= read -r line; do
        printf "%b\n" "$line" >>"$out"
    done
    echo "$out"
}

function get_current() {
    local out=$WORK_DIR/${FUNCNAME[0]}
    >"$out"
    $WPA_CLI status 2>/dev/null | while IFS= read -r line; do
        printf "%b\n" "$line" >>"$out"
    done
    echo "$out"
}

function get_eth() {
    local ifname="eth0"
    local ip=$(_ip "$ifname")
    local mask=$(_mask "$ifname")
    local mac=$(_mac "$ifname")
    local gateway=$(_gateway "$ifname")
    printf '{"ip": "%s", "mask": "%s", "mac": "%s", "gateway": "%s", "dns1": "", "dns2": ""}' \
        "$ip" "$mask" "$mac" "$gateway"
}

function _get_scan_results() {
    local out=$WORK_DIR/${FUNCNAME[0]}
    >"$out"
    for ((i = 1; i <= 5; i++)); do
        if [[ "$($WPA_CLI scan 2>/dev/null)" == "OK" ]]; then
            break
        fi
        sleep 0.5
    done

    sleep 3
    $WPA_CLI scan_results 2>/dev/null | while IFS= read -r line; do
        printf "%b\n" "$line" >>"$out"
    done
}

function getWiFiScanResults() {
    local out=$WORK_DIR/${FUNCNAME[0]}
    local result="$WORK_DIR/_get_scan_results"

    if [ -s "$result" ]; then
        cp "$result" "$out"
    fi
    _get_scan_results >/dev/null 2>&1 &

    [ -f "$out" ] || >"$out"
    echo "$out"
}

function connectWiFi() {
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

    local ids=$(cat $(get_networks) | awk -F'\t' '{print $1}')
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

function _wpa_set_networks() {
    local ssid="$1"
    local status="$2"
    $WPA_CLI reconfigure >/dev/null 2>&1
    local id=$(cat $(get_networks) | grep -w "$ssid" | awk -F'\t' '{print $1}')
    [ -z "$id" ] && return 0
    $WPA_CLI "$status" "$id" >/dev/null 2>&1
    $WPA_CLI save_config
}

function disconnectWiFi() {
    _wpa_set_networks "$2" disable_network
}

function forgetWiFi() {
    _wpa_set_networks "$2" remove_network
}

# tmp work dir
if [ ! -d $WORK_DIR ]; then
    mkdir -p $WORK_DIR
    chmod 400 -R $WORK_DIR
fi

# call function
$1 "$@"
