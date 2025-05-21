USER_NAME=recamera
SSH_DIR="/home/$USER_NAME/.ssh"
SSH_KEY_FILE="$SSH_DIR/authorized_keys"

# function _check_err()
# {
#     if [ 0 -ne $1 ]; then
#         echo "$1"
#         exit 1
#     fi
# }

# function _success()
# {
#     echo "0"
#     exit 0
# }

function _is_key_valid()
{
    local tmp=$(mktemp)
    echo "$*">$tmp
    ssh-keygen -lf $tmp >/dev/null 2>&1
    local ret=$?
    rm $tmp
    return $ret
}

function _is_key_exist()
{
    grep -e "$*" $SSH_KEY_FILE >/dev/null 2>&1
    return $?
}

function genToken()
{
    dd if=/dev/random bs=32 count=1 2>/dev/null | base64 -w0
}

function addSShkey()
{
    local name="$2"
    local time="$3"
    shift 3
    local key="$*"

    _is_key_valid $key
    if [ 0 -ne $? ]; then
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

    echo "$key # $name $time" >> $filename
    echo "OK"
}

case $1 in
    genToken)
        $1 $@
    ;;

    addSShkey)
        $1 $@
    ;;
esac