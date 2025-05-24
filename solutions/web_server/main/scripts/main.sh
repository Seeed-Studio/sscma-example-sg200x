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

PATH_UPGRADE_URL="/etc/upgrade"
DEFAULT_UPGRADE_URL="https://github.com/Seeed-Studio/reCamera-OS/releases/latest"

USER_DIR="/userdata"
APP_DIR="$USER_DIR/app"
MODEL_DIR="$USER_DIR/MODEL"
MODEL_SUFFIX=".cvimodel"
MODEL_FILE="$MODEL_DIR/model${MODEL_SUFFIX}"
MODEL_INFO="$MODEL_DIR/model.json"

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
    local ifname=$2
    wpa_cli -i $ifname status 2>/dev/null | grep "^wpa_state" | awk -F= '{print $2}'
}

function _ip() {
    local ifname=$2
    ifconfig $ifname 2>/dev/null | awk '/inet addr:/ {print $2}' | awk -F':' '{print $2}'
}

function _mask() {
    local ifname=$2
    ifconfig $ifname 2>/dev/null | awk '/Mask:/ {print $4}' | awk -F':' '{print $2}'
}

function _mac() {
    local ifname=$2
    ifconfig $ifname 2>/dev/null | awk '/HWaddr/ {print $5}'
}

function _gateway() {
    local ifname=$2
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

    local ip=""
    local mask=""
    local mac=""
    local gateway=""
    local dns="-"

    local ifname="wlan0"
    if [ $(_wifi_status $1 $ifname) = "COMPLETED" ]; then
        ip=$(_ip $1 $ifname)
        mask=$(_mask $1 $ifname)
        mac=$(_mac $1 $ifname)
        gateway=$(_gateway $1 $ifname)
    fi
    [ -z $ip ] && ip="-"
    [ -z $mask ] && mask="-"
    [ -z $mac ] && mac="-"
    [ -z $gateway ] && gateway="-"
    dns=$gateway

    local ch=$(cat $PATH_UPGRADE_URL 2>/dev/null | awk -F',' '{print $1}')
    local url=$(cat $PATH_UPGRADE_URL 2>/dev/null | awk -F',' '{print $2}')
    [ -z $ch ] && ch=0

    local os_name=$(_os_name)
    local os_version=$(_os_version)

    local upgraded=0

    printf '{"deviceName": "%s", "sn": "%s",
"wifiIp": "%s", "mask": "%s", "mac": "%s", "gateway": "%s", "dns": "%s",
"channel": %s, "serverUrl": "%s", "officialUrl": "%s",
"osName": "%s", "osVersion": "%s",
"cpu": "%s", "ram": %s, "npu": %s,
"terminalPort": %s, "needRestart": %s
}' \
        "${dev_name}" "${sn}" \
        "${ip}" "${mask}" "${mac}" "${gateway}" "${dns}" \
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
    echo $ch,$url >$PATH_UPGRADE_URL
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
    echo $PLATFORM_INFO
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
    local file=$WORK_DIR/avahi-browse.out
    local tmp=$WORK_DIR/avahi-browse.tmp
    if [ -f $file ]; then
        cp $file $tmp
        rm $file
    fi
    avahi-browse -arpt >$file &

    [ ! -f $tmp ] && >$tmp
    printf '{"file": "%s"}' $tmp
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

# tmp work dir
if [ ! -d $WORK_DIR ]; then
    mkdir -p $WORK_DIR
    chmod 400 -R $WORK_DIR
fi

# call function
if [ "$(type $1)" == "$1 is a function" ]; then
    $1 $@
else
    echo "Not found"
fi
