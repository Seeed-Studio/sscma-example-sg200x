WORK_DIR=/tmp/web_server
USER_NAME=recamera
SSH_DIR="/home/$USER_NAME/.ssh"
SSH_KEY_FILE="$SSH_DIR/authorized_keys"

FIRST_LOGIN="/etc/.first_login"

INID_DIR="/etc/init.d"
INID_DIR_DISABLED="/etc/init.d/disabled"

SSH_SERVICE="S*sshd"

function _is_key_valid()
{
    local tmp=$(mktemp)
    local ret=0;

    echo "$*">$tmp
    ssh-keygen -lf $tmp 2>/dev/null
    ret=$?
    rm $tmp
    return $ret;
}

function _is_key_exist()
{
    # Todo: check sha256
    grep -e "$*" $SSH_KEY_FILE >/dev/null 2>&1
    return $?
}

function gen_token()
{
    dd if=/dev/random bs=64 count=1 2>/dev/null | base64 -w0
}

function get_username()
{
    echo $USER_NAME
}

function _update_sshkey()
{
    local sshKeyFile=$WORK_DIR/sshkey
    > $sshKeyFile
    if [ -f $SSH_KEY_FILE ]; then
        local cnt=1
        while IFS= read -r line; do
            if [[ "$line" =~ ^#.*$ ]]; then # skip comment line
                continue
            fi

            local sh256=$(_is_key_valid $line)
            if [ -n "$sh256" ]; then
                echo "$cnt $sh256" >> $sshKeyFile
                let cnt++
            fi
        done < $SSH_KEY_FILE
    fi
}

function addSShkey()
{
    local name="$2"
    local time="$3"
    shift 3
    local key="$*"

    _is_key_valid $key > /dev/null
    if [ $? -ne 0 ]; then
        echo "invalid"
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
        echo "exist"
        exit 1
    fi

    echo "$key  # $name $time" >> $filename
    # _update_sshkey

    echo "OK"
}

function deleteSShkey()
{
    local line="$2"

    if ! [[ "$line" =~ ^[0-9]+$ ]]; then
        echo "invalid id=$line"
        exit 1
    fi

    total_lines=$(wc -l < "$SSH_KEY_FILE" 2>/dev/null || echo 0)
    if [ "$line" -lt 1 ] || [ "$line" -gt "$total_lines" ]; then
        echo "invalid id=$line"
        exit 1
    fi

    # Todo: skip comment line
    sed -i "${line}d" "$SSH_KEY_FILE"
    # _update_sshkey

    echo "OK"
}

function login()
{
    [ -f $FIRST_LOGIN ] && rm -f $FIRST_LOGIN
}

function queryUserInfo()
{
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

function setSShStatus()
{
    local dir="$INID_DIR"
    local dir_disabled="$INID_DIR_DISABLED"
    local sshd="$SSH_SERVICE"
    local ret=0;

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
        echo "Failed"
    else
        echo "OK"
    fi
}

function updatePassword()
{
    passwd $USER_NAME 1>/dev/null 2>/dev/null << EOF
$3
$3
EOF
    if [ 0 -ne $? ]; then
        echo "Failed"
    else
        echo "OK"
    fi
}

# tmp work dir
if [ ! -d $WORK_DIR ]; then
    mkdir -p $WORK_DIR
    chmod 400 -R $WORK_DIR
fi

# call function
if [ "$(type $1)"  == "$1 is a function" ]; then
    $1 $@
else
    echo "$1 not found"
fi
